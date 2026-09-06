#include "commands/CallGraphInvalidation.h"

#include "commands/ConfigurationSupport.h"
#include "commands/DatabasePaths.h"
#include "commands/FactPairValidation.h"
#include "storage/FactStore.h"
#include "storage/catalog/File.h"
#include "storage/catalog/Database.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <vector>

namespace facts::commands {
std::expected<void, std::string> invalidateConfiguredCallGraphEntries(
    const config::Resolved &resolved, const std::string &explicitFacts,
    const std::vector<std::string> &sources) {
  std::vector<std::string> paths;
  if (explicitFacts.empty() && !std::filesystem::is_regular_file(resolved.database)) return {};
  if (!explicitFacts.empty()) {
    paths.push_back(explicitFacts);
  } else if (!resolved.factsTemplate.empty()) {
    const bool perSource = resolved.factsTemplate.find("{relative_path}") !=
                               std::string::npos ||
                           resolved.factsTemplate.find("{filename}") !=
                               std::string::npos;
    std::vector<std::string> selected = sources;
    if (perSource && selected.empty() &&
        std::filesystem::is_regular_file(resolved.database)) {
      auto database = catalog::open(resolved.database.string(), false);
      if (!database) return std::unexpected(database.error());
      auto commandIds = catalog::query(
          *database, "SELECT id FROM file WHERE compile_options IS NOT NULL",
          [](const storage::Row &row) { return row.get<FileId>(0); });
      if (!commandIds) return std::unexpected(commandIds.error());
      auto files = catalog::files(*database);
      if (!files) return std::unexpected(files.error());
      for (const auto &file : *files) {
        if (std::ranges::find(*commandIds, file.id) == commandIds->end())
          continue;
        auto path = catalog::filePath(file);
        if (!path) return std::unexpected(path.error());
        selected.push_back(path->string());
      }
    }
    if (perSource) {
      for (const auto &source : selected) {
        auto path = resolveFactsOutput(resolved, {source});
        if (!path) return std::unexpected(path.error());
        paths.push_back(path->string());
      }
    } else {
      auto path = resolveFactsOutput(resolved, {});
      if (!path) return std::unexpected(path.error());
      paths.push_back(path->string());
    }
  }
  std::ranges::sort(paths);
  paths.erase(std::ranges::unique(paths).begin(), paths.end());
  for (const auto &path : paths) {
    auto validPaths = validateDatabasePaths(path, resolved.database.string());
    if (!validPaths) return validPaths;
    if (!std::filesystem::exists(path) ||
        !std::filesystem::is_regular_file(resolved.database))
      continue;
    auto invalidated = invalidateCallGraphEntriesBeforeMutation(
        resolved.database.string(), path);
    if (!invalidated) return invalidated;
  }
  return {};
}

std::expected<void, std::string>
invalidateCallGraphEntriesBeforeMutation(const std::string &configuration,
                                         const std::string &facts) {
  if (facts.empty() || !std::filesystem::exists(facts))
    return {};
  return validateFactPairForRead(facts, configuration).and_then([&] {
    return catalog::open(configuration, false);
  }).and_then([](auto database) {
    return catalog::query(
        database, "SELECT id FROM file ORDER BY id",
        [](const storage::Row &row) { return row.get<FileId>(0); });
  }).and_then([&facts](auto ids) -> std::expected<void, std::string> {
    try {
      FactStore store(facts);
      return store.invalidateCallGraphEntries(ids)
          .transform_error([](std::error_code error) {
            return "cannot invalidate call graph entries: " + error.message();
          });
    } catch (const std::exception &error) {
      return std::unexpected("cannot invalidate call graph entries: " +
                             std::string(error.what()));
    }
  });
}

} // namespace facts::commands
