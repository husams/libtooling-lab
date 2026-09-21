#include "apis/watch/plan/Details.h"
#include "apis/watch/CompilationValidation.h"
#include <map>
#include "tooling/StoredCompilationReader.h"
#include "tooling/ImportCompilationDatabase.h"

namespace facts::apis::watch::plan {
std::expected<std::vector<Compilation>, std::string>
compilations(const std::vector<std::filesystem::path> &directories) {
  std::vector<Compilation> result;
  using Signatures = std::set<std::vector<std::string>>;
  std::map<std::filesystem::path, std::pair<std::filesystem::path, Signatures>> owners;
  for (const auto &directory : std::set(directories.begin(), directories.end())) {
    auto database = loadImportCompilationDatabase(directory);
    if (!database) return std::unexpected(database.error());
    Compilation compilation{directory, {}};
    std::map<std::filesystem::path, Signatures> signatures;
    for (const auto &command : (*database)->getAllCompileCommands()) {
      const auto working = std::filesystem::path(command.Directory);
      const auto source = std::filesystem::path(command.Filename);
      compilation.sources.push_back(source.string());
      std::error_code identityError;
      auto identity = std::filesystem::canonical(source, identityError);
      auto signature = command.CommandLine;
      signature.insert(signature.begin(), working.lexically_normal().string());
      signature.push_back(command.Output);
      signatures[identityError ? source : identity].insert(std::move(signature));
    }
    for (auto &[source, values] : signatures) {
      const auto previous = owners.find(source);
      if (previous != owners.end() && previous->second.second != values)
        return std::unexpected("conflicting compilation databases for " + source.string() +
            ": " + (previous->second.first / "compile_commands.json").string() +
            " and " + (directory / "compile_commands.json").string() +
            "; select one compilation database explicitly");
      owners.try_emplace(source, directory, std::move(values));
    }
    result.push_back(std::move(compilation));
  }
  return result;
}

std::expected<std::vector<std::string>, std::string>
storedSources(const Catalog &catalog) {
  if (catalog.clones.empty()) return std::vector<std::string>{};
  return openStoredDatabase(catalog.database).and_then([](StoredDatabase database) {
    return readStoredCompilation(database.get()).transform([](auto snapshot) {
      std::vector<std::string> sources;
      for (const auto &file : snapshot.files) sources.push_back(file.path.string());
      return sources;
    });
  });
}
}

namespace facts::apis::watch {
std::expected<void, std::string> validateCompilationDatabases(
    const std::set<std::filesystem::path> &directories) {
  return plan::compilations({directories.begin(), directories.end()})
      .transform([](const auto &) {});
}
}
