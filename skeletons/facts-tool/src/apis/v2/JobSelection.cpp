#include "apis/v2/JobServices.h"
#include "storage/catalog/File.h"
#include <algorithm>
#include <map>
#include <set>

namespace facts::apis::v2::jobs {
namespace {
bool within(const std::filesystem::path &path, const std::filesystem::path &root) {
  const auto relative = path.lexically_normal().lexically_relative(root.lexically_normal());
  return !relative.empty() && !relative.is_absolute() && *relative.begin() != "..";
}
Result<std::map<std::int64_t, std::string>> repositories(catalog::Database &database) {
  return catalog::query(database, "SELECT id,name FROM repository",
      [](const storage::Row &row) { return std::pair{row.integer(0), row.string(1)}; })
      .transform([](const auto &rows) { return std::map<std::int64_t, std::string>{rows.begin(), rows.end()}; })
      .transform_error(failed);
}
Result<domain::FileSelector> selector(const catalog::File &file,
                                     const std::map<std::int64_t, std::string> &repos) {
  return catalog::filePath(file).transform_error(failed).transform([&](const auto &path) {
    domain::FileSelector result{path.string()};
    result.component = file.component.name;
    if (file.component.repositoryId && repos.contains(*file.component.repositoryId))
      result.repo = repos.at(*file.component.repositoryId);
    return result;
  });
}
Result<domain::FileSelector> explicitSelector(const Json &value,
    const std::vector<catalog::File> &files,
    const std::map<std::int64_t, std::string> &repos) {
  domain::FileSelector result;
  if (value.contains("file_id")) {
    const auto found = std::ranges::find(files, std::stoll(value["file_id"].get<std::string>()), &catalog::File::id);
    if (found == files.end())
      return std::unexpected(domain::Error{404, "file_not_found", "Unknown file_id"});
    auto selected = selector(*found, repos);
    if (!selected) return selected;
    result = std::move(*selected);
  } else result.path = value["path"].get<std::string>();
  if (value.contains("repository")) result.repo = value["repository"].get<std::string>();
  if (value.contains("component")) result.component = value["component"].get<std::string>();
  if (value.contains("clone")) result.clone = value["clone"].get<std::string>();
  return result;
}
bool matches(const catalog::File &file, const domain::FileSelector &candidate,
             const Json &selection) {
  if (selection.contains("repository") && candidate.repo.value_or("") != selection["repository"].get<std::string>())
    return false;
  if (selection.contains("component") && candidate.component.value_or("") != selection["component"].get<std::string>())
    return false;
  if (selection["type"] != "directory") return true;
  auto directory = std::filesystem::path(selection["path"].get<std::string>());
  if (directory.is_relative())
    directory = (file.clone ? std::filesystem::path(file.clone->path) :
                             std::filesystem::path(file.component.path)) / directory;
  return within(candidate.path, directory);
}
}
Result<std::vector<domain::ResolvedFile>> selectFiles(const domain::Context &context,
                                                     const Json &selection) {
  return catalog::open(context.configuration.database.string(), false)
      .transform_error(failed).and_then([&](catalog::Database database)
          -> Result<std::vector<domain::ResolvedFile>> {
        auto files = catalog::files(database).transform_error(failed);
        if (!files) return std::unexpected(files.error());
        auto repos = repositories(database);
        if (!repos) return std::unexpected(repos.error());
        std::vector<domain::FileSelector> selections;
        if (selection["type"] == "files") {
          for (const auto &value : selection["files"]) {
            auto selected = explicitSelector(value, *files, *repos);
            if (!selected) return std::unexpected(selected.error());
            selections.push_back(std::move(*selected));
          }
        } else for (const auto &file : *files) {
          // Bulk analysis selects translation units. Explicit file selection may
          // still select a header, for which prepareCompilation finds its owner.
          if (file.driver.empty()) continue;
          auto selected = selector(file, *repos);
          if (!selected) return std::unexpected(selected.error());
          if (matches(file, *selected, selection)) selections.push_back(std::move(*selected));
        }
        std::vector<domain::ResolvedFile> result;
        std::set<std::pair<std::int64_t, std::string>> seen;
        for (const auto &selected : selections) {
          auto file = domain::resolveFile(context, selected, false);
          if (!file) return std::unexpected(file.error());
          if (seen.emplace(file->fileId, file->path.string()).second)
            result.push_back(std::move(*file));
        }
        if (result.empty()) return std::unexpected(domain::Error{
            404, "no_sources", "Selection contains no registered translation units"});
        return result;
      });
}
Result<watch::Catalog> selectClones(const domain::Context &context,
                                   const runtime::Request &request) {
  auto settings = request.settings;
  settings.defaults.insert(settings.defaults.end(), {"--conf", context.configuration.database.string()});
  return watch::readCatalog(settings).transform_error(failed)
      .and_then([&](watch::Catalog catalog) -> Result<watch::Catalog> {
        const auto selection = request.options.value("selection", Json{{"type", "all"}});
        const auto repository = request.options.value("repository", selection.value("repository", std::string{}));
        std::erase_if(catalog.clones, [&](const auto &clone) {
          return !clone.active || (!repository.empty() && clone.repository != repository);
        });
        if (catalog.clones.empty()) return std::unexpected(domain::Error{
            404, "repository_not_found", "No active registered clone matches the selection"});
        if (selection["type"] == "directory") {
          for (auto &clone : catalog.clones) {
            auto path = std::filesystem::path(selection["path"].get<std::string>());
            if (path.is_relative()) path = clone.path / path;
            if (!within(path, clone.path)) clone.active = false;
            else clone.path = path.lexically_normal();
          }
          std::erase_if(catalog.clones, [](const auto &clone) { return !clone.active; });
          if (catalog.clones.empty()) return std::unexpected(domain::Error{
              404, "directory_not_found", "Directory is outside the selected registered clones"});
        } else if (selection["type"] == "component" || selection["type"] == "files") {
          return std::unexpected(invalid("scan/import selection must be all, repository, or directory"));
        }
        return catalog;
      });
}
}
