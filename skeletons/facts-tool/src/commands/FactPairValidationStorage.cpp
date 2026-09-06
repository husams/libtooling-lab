#include "commands/FactPairValidationInternal.h"

#include "storage/Sqlite.h"

#include <vector>

namespace facts::commands::detail {
namespace {
std::expected<std::map<FileId, Provenance>, std::string>
storedProvenance(storage::Database &database) {
  auto rows = database.query(
      "SELECT file_id,path,universe_key FROM facts_project_provenance",
      [](const storage::Row &row) {
        return std::pair<FileId, Provenance>{row.get<FileId>(0),
                                             {row.string(1), row.string(2)}};
      });
  std::map<FileId, Provenance> result;
  try {
    for (auto value : rows) {
      result.emplace(value.first, std::move(value.second));
    }
  } catch (const storage::QueryError &error) {
    return std::unexpected(error.what());
  }
  return result;
}

std::string mismatch(std::string_view reason, FileId id) {
  return "incompatible-symbol-universe: " + std::string(reason) + ": " +
         std::to_string(id);
}
} // namespace

std::expected<void, std::string>
validateFactsProvenance(storage::Database &database,
                        const ProvenanceSnapshot &snapshot, bool writing) {
  auto present = factsTableExists(database, "facts_project_provenance");
  if (!present) {
    return std::unexpected(present.error());
  }
  if (!*present && !writing) {
    return {};
  }
  auto known = *present
                   ? storedProvenance(database)
                   : std::expected<std::map<FileId, Provenance>, std::string>{
                         std::map<FileId, Provenance>{}};
  if (!known) {
    return std::unexpected(known.error());
  }
  auto used = usedFactFiles(database);
  if (!used) {
    return std::unexpected(used.error());
  }
  if (known->empty() && !used->empty() && writing) {
    return std::unexpected(
        "incompatible-symbol-universe: facts store has no provenance: "
        "write to a new facts file and extract every source");
  }
  for (const auto id : *used) {
    auto knownValue = known->find(id);
    if (knownValue == known->end()) {
      if (writing) {
        return std::unexpected(mismatch("facts symbol has no provenance", id));
      }
      continue;
    }
    auto current = snapshot.find(id);
    if (current == snapshot.end() ||
        current->second.path != knownValue->second.path ||
        current->second.universe != knownValue->second.universe) {
      return std::unexpected(mismatch("facts file identity differs", id));
    }
  }
  return {};
}

} // namespace facts::commands::detail
