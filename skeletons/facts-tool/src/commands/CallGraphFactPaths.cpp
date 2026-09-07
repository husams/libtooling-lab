#include "commands/CallGraphInvalidation.h"

#include "commands/ConfigurationSupport.h"
#include "storage/catalog/Database.h"
#include "storage/catalog/File.h"

#include <algorithm>
#include <filesystem>

namespace facts::commands {
namespace {
std::expected<std::vector<std::string>, std::string>
registeredSources(const config::Resolved &resolved) {
  auto database = catalog::open(resolved.database.string(), false);
  if (!database)
    return std::unexpected(database.error());
  auto commandIds = catalog::query(
      *database, "SELECT id FROM file WHERE compile_options IS NOT NULL",
      [](const storage::Row &row) { return row.get<FileId>(0); });
  if (!commandIds)
    return std::unexpected(commandIds.error());
  auto files = catalog::files(*database);
  if (!files)
    return std::unexpected(files.error());
  std::vector<std::string> sources;
  for (const auto &file : *files) {
    if (std::ranges::find(*commandIds, file.id) == commandIds->end())
      continue;
    auto path = catalog::filePath(file);
    if (!path)
      return std::unexpected(path.error());
    sources.push_back(path->string());
  }
  return sources;
}
} // namespace

std::expected<std::vector<std::string>, std::string>
configuredCallGraphFactPaths(const config::Resolved &resolved,
                             const std::string &explicitFacts,
                             const std::vector<std::string> &sources) {
  if (!explicitFacts.empty())
    return std::vector{explicitFacts};
  if (!std::filesystem::is_regular_file(resolved.database))
    return std::vector<std::string>{};
  auto populated = callGraphProjectHasFiles(resolved.database.string());
  if (!populated)
    return std::unexpected(populated.error());
  if (!*populated)
    return std::vector<std::string>{};
  if (resolved.factsTemplate.empty()) {
    return std::unexpected(
        "cannot invalidate call graph entries: paired facts store is "
        "unknown; supply --facts PATH or configure facts_template "
        "before mutating an existing project");
  }
  const bool perSource =
      resolved.factsTemplate.find("{relative_path}") != std::string::npos ||
      resolved.factsTemplate.find("{filename}") != std::string::npos;
  if (!perSource)
    return resolveFactsOutput(resolved, {}).transform([](const auto &path) {
      return std::vector{path.string()};
    });
  auto selected =
      sources.empty()
          ? registeredSources(resolved)
          : std::expected<std::vector<std::string>, std::string>{sources};
  if (!selected)
    return std::unexpected(selected.error());
  if (selected->empty())
    return std::unexpected("cannot resolve source-based facts_template without "
                           "registered compile commands; supply --facts PATH");
  std::vector<std::string> paths;
  for (const auto &source : *selected) {
    auto path = resolveFactsOutput(resolved, {source});
    if (!path)
      return std::unexpected(path.error());
    paths.push_back(path->string());
  }
  std::ranges::sort(paths);
  paths.erase(std::ranges::unique(paths).begin(), paths.end());
  return paths;
}
} // namespace facts::commands
