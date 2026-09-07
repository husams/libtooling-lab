#include "commands/FactPairValidationInternal.h"

#include "storage/catalog/File.h"

#include <filesystem>
#include <map>
#include <utility>

namespace facts::commands::detail {

std::expected<ProvenanceSnapshot, std::string>
loadProjectProvenance(const std::string &path) {
  auto database = storage::Database::open(path, storage::Database::readOnly);
  if (!database) {
    return std::unexpected("cannot open project configuration: " +
                           database.error().message());
  }
  auto files = catalog::files(*database);
  if (!files) {
    return std::unexpected("cannot read project file registry: " +
                           files.error());
  }
  auto keys = catalog::query(
      *database,
      "SELECT c.id,coalesce(su.key,'legacy') FROM component c LEFT JOIN "
      "semantic_universe su ON su.id=c.semantic_universe_id",
      [](const storage::Row &row) {
        return std::pair<FileId, std::string>{
            static_cast<FileId>(row.integer(0)), row.string(1)};
      });
  if (!keys) {
    return std::unexpected("cannot read project universe: " + keys.error());
  }
  std::map<FileId, std::string> universes;
  for (auto &key : *keys) {
    universes.emplace(key.first, std::move(key.second));
  }
  ProvenanceSnapshot result;
  for (const auto &file : *files) {
    auto full = catalog::filePath(file);
    if (!full) {
      return std::unexpected("cannot resolve project file: " + full.error());
    }
    std::error_code error;
    auto canonical = std::filesystem::weakly_canonical(*full, error);
    if (error) {
      return std::unexpected("cannot canonicalize project file: " +
                             error.message());
    }
    auto key = universes.find(static_cast<FileId>(file.component.id));
    result.emplace(static_cast<FileId>(file.id),
                   Provenance{canonical.string(),
                              key == universes.end() ? "legacy" : key->second});
  }
  return result;
}

} // namespace facts::commands::detail
