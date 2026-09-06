#include "commands/CallGraphInvalidation.h"

#include "commands/ConfigurationSupport.h"
#include "commands/DatabasePaths.h"
#include "commands/FactPairValidation.h"
#include "storage/FactStore.h"
#include "storage/catalog/Database.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <vector>

namespace facts::commands {
std::expected<void, std::string>
invalidateConfiguredCallGraphEntries(const config::Resolved &resolved,
                                     const std::string &explicitFacts,
                                     const std::vector<std::string> &sources) {
  auto paths = configuredCallGraphFactPaths(resolved, explicitFacts, sources);
  if (!paths)
    return std::unexpected(paths.error());
  for (const auto &path : *paths) {
    auto validPaths = validateDatabasePaths(path, resolved.database.string());
    if (!validPaths)
      return validPaths;
    if (!std::filesystem::exists(path) ||
        !std::filesystem::is_regular_file(resolved.database))
      continue;
    auto invalidated = invalidateCallGraphEntriesBeforeMutation(
        resolved.database.string(), path);
    if (!invalidated)
      return invalidated;
  }
  return {};
}

std::expected<void, std::string>
invalidateCallGraphEntriesBeforeMutation(const std::string &configuration,
                                         const std::string &facts) {
  if (facts.empty() || !std::filesystem::exists(facts))
    return {};
  return validateFactPairForRead(facts, configuration)
      .and_then([&] { return catalog::open(configuration, false); })
      .and_then([](auto database) {
        return catalog::query(
            database, "SELECT id FROM file ORDER BY id",
            [](const storage::Row &row) { return row.get<FileId>(0); });
      })
      .and_then([&facts](auto ids) -> std::expected<void, std::string> {
        try {
          FactStore store(facts);
          return store.invalidateCallGraphEntries(ids).transform_error(
              [](std::error_code error) {
                return "cannot invalidate call graph entries: " +
                       error.message();
              });
        } catch (const std::exception &error) {
          return std::unexpected("cannot invalidate call graph entries: " +
                                 std::string(error.what()));
        }
      });
}

} // namespace facts::commands
