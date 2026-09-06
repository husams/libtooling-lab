#pragma once

#include "storage/SqliteDatabase.h"

#include <expected>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace facts::commands::detail {

struct Provenance {
  std::string path;
  std::string universe;
};

using ProvenanceSnapshot = std::map<FileId, Provenance>;

std::expected<ProvenanceSnapshot, std::string>
loadProjectProvenance(const std::string &path);

std::expected<void, std::string>
validateFactsProvenance(storage::Database &database,
                        const ProvenanceSnapshot &snapshot, bool writing);

std::expected<int, std::string> factsSchemaVersion(storage::Database &database);
std::expected<bool, std::string> factsTableExists(storage::Database &database,
                                                  std::string_view name);
std::expected<std::set<FileId>, std::string>
usedFactFiles(storage::Database &database);

} // namespace facts::commands::detail
