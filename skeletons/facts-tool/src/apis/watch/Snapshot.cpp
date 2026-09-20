#include "apis/watch/Snapshot.h"
#include "config/Configuration.h"
#include "apis/watch/plan/Details.h"
#include "tooling/StoredCompilationReader.h"
#include "storage/FileIndexState.h"
#include <sqlite3.h>
#include <llvm/Support/SHA256.h>
#include <llvm/ADT/StringExtras.h>
#include <nlohmann/json.hpp>
#include <array>
#include <algorithm>
#include <fstream>

namespace facts::apis::watch {
namespace {
std::filesystem::path checkpoint(const Settings &settings) {
  return settings.serverConfig.string() + ".watch-state.json";
}
bool readableFacts(const std::filesystem::path &path) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error) || error ||
      std::filesystem::file_size(path, error) == 0 || error) return false;
  sqlite3 *database = nullptr;
  if (sqlite3_open_v2(path.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
    if (database) sqlite3_close(database);
    return false;
  }
  sqlite3_stmt *statement = nullptr;
  const bool valid = sqlite3_prepare_v2(database,
      "SELECT count(*) FROM sqlite_master WHERE type='table'", -1, &statement, nullptr) == SQLITE_OK &&
      sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) > 0;
  if (statement) sqlite3_finalize(statement);
  sqlite3_close(database);
  return valid;
}
bool indexedInputs(const Settings &settings, const Scan &scan) {
  auto database = openStoredDatabase(scan.catalog.database);
  if (!database) return false;
  auto stored = readStoredCompilation(database->get());
  if (!stored) return false;
  std::map<std::string, FileId> identities;
  std::vector<std::string> paths;
  for (const auto &file : stored->files) {
    paths.push_back(file.path.string());
    identities.emplace(file.path.string(), file.id);
  }
  std::set<std::filesystem::path> outputs;
  for (const auto &clone : scan.catalog.clones) {
    if (!clone.active || !clone.excluded.empty()) continue;
    auto ignore = Ignore::create(clone.path, settings);
    if (!ignore) return false;
    auto selected = plan::selectSources(paths, scan.catalog, clone, *ignore, settings);
    if (!selected) return false;
    for (const auto &path : *selected) {
      auto state = readFileIndexState(database->get(), identities.at(path));
      if (!state || !state->indexed || state->factsDb.empty()) return false;
      outputs.insert(state->factsDb);
    }
  }
  return std::ranges::all_of(outputs, readableFacts);
}
std::string digest(std::string_view text) {
  llvm::SHA256 hash;
  hash.update(llvm::StringRef(text.data(), text.size()));
  const auto bytes = hash.final();
  return llvm::toHex(llvm::ArrayRef(bytes), true);
}
}
std::expected<std::string, std::string> fingerprint(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::unexpected("cannot read scan input: " + path.string());
  llvm::SHA256 hash;
  std::array<char, 65536> buffer;
  while (input) {
    input.read(buffer.data(), buffer.size());
    hash.update(llvm::StringRef(buffer.data(), static_cast<std::size_t>(input.gcount())));
  }
  if (!input.eof()) return std::unexpected("cannot finish reading scan input: " + path.string());
  const auto bytes = hash.final();
  return llvm::toHex(llvm::ArrayRef(bytes), true);
}
std::string signature(const Settings &settings, const Scan &scan) {
  nlohmann::json clones = nlohmann::json::array(), inputs = nlohmann::json::array();
  for (const auto &clone : scan.catalog.clones)
    clones.push_back({clone.repositoryId, clone.cloneId, clone.repository, clone.label,
                      clone.path.string(), clone.active, clone.excluded});
  for (const auto &[path, value] : scan.inputs) inputs.push_back({path.string(), value});
  config::Request request;
  request.direct = scan.catalog.database.string();
  for (std::size_t i = 0; i + 1 < settings.defaults.size(); ++i)
    if (settings.defaults[i] == "--config") request.selector = settings.defaults[i + 1];
  const auto resolved = config::resolve(request);
  nlohmann::json configuration = resolved
      ? nlohmann::json{{"facts_template", resolved->factsTemplate}, {"extra_args", resolved->extraArguments},
                       {"ast_cache", resolved->astCache.enabled}, {"ast_directory", resolved->astCache.directory.string()}}
      : nlohmann::json{{"error", resolved.error()}};
  std::error_code executableError;
  const auto executableTime = std::filesystem::last_write_time(settings.executable, executableError);
  return digest(nlohmann::json{{"version", 1}, {"database", scan.catalog.database.string()},
      {"working_directory", settings.workingDirectory.string()},
      {"executable", settings.executable.string()},
      {"executable_time", executableError ? 0 : executableTime.time_since_epoch().count()},
      {"clones", clones}, {"compilation", scan.catalog.compilationState}, {"inputs", inputs},
      {"configuration", configuration}, {"defaults", settings.defaults}, {"imports", settings.importArguments},
      {"extracts", settings.extractArguments}, {"excluded_directories", settings.excludedDirectories},
      {"exclude_patterns", settings.excludePatterns}}.dump());
}
bool restored(const Settings &settings, const Scan &scan) {
  if (settings.serverConfig.empty() || !scan.notices.empty() || scan.signature.empty()) return false;
  std::ifstream input(checkpoint(settings));
  if (!input) return false;
  const auto value = nlohmann::json::parse(input, nullptr, false);
  return value.is_object() && value.contains("signature") && value.at("signature").is_string() &&
         value.at("signature").get<std::string>() == scan.signature && indexedInputs(settings, scan);
}
std::expected<void, std::string> forget(const Settings &settings) {
  if (settings.serverConfig.empty()) return {};
  std::error_code error;
  std::filesystem::remove(checkpoint(settings), error);
  return error ? std::unexpected("cannot invalidate watcher checkpoint: " + error.message())
               : std::expected<void, std::string>{};
}
std::expected<void, std::string> remember(const Settings &settings, const Scan &scan) {
  if (settings.serverConfig.empty() || !scan.notices.empty() || scan.signature.empty()) return {};
  const auto path = checkpoint(settings);
  const auto temporary = path.string() + ".tmp";
  std::ofstream output(temporary, std::ios::trunc);
  if (!output) return std::unexpected("cannot write watcher checkpoint");
  output << nlohmann::json{{"version", 1}, {"signature", scan.signature}}.dump() << '\n';
  output.close();
  if (!output) return std::unexpected("cannot finish watcher checkpoint");
  std::error_code error;
  std::filesystem::rename(temporary, path, error);
  return error ? std::unexpected("cannot replace watcher checkpoint: " + error.message())
               : std::expected<void, std::string>{};
}
}
