#include "commands/FactPairValidation.h"

#include "commands/FactPairValidationInternal.h"

#include <filesystem>

namespace facts::commands {

std::expected<bool, std::string>
legacyFactsNeedRegistration(const std::string &facts,
                            const std::string &project) {
  std::error_code error;
  if (!std::filesystem::exists(facts, error)) {
    if (error) {
      return std::unexpected("cannot inspect facts database: " +
                             error.message());
    }
    return false;
  }
  auto snapshot = detail::loadProjectProvenance(project);
  if (!snapshot)
    return std::unexpected(snapshot.error());
  auto database = storage::Database::open(facts, storage::Database::readOnly);
  if (!database)
    return std::unexpected("cannot open facts database: " +
                           database.error().message());
  if (auto version = detail::factsSchemaVersion(*database); !version)
    return std::unexpected(version.error());
  auto present =
      detail::factsTableExists(*database, "facts_project_provenance");
  if (!present)
    return std::unexpected(present.error());
  if (*present) {
    auto rows =
        database->query("SELECT COUNT(*) FROM facts_project_provenance",
                        [](const storage::Row &row) { return row.integer(0); });
    try {
      for (const auto count : rows)
        if (count != 0)
          return false;
    } catch (const storage::QueryError &error) {
      return std::unexpected(error.what());
    }
  }
  auto used = detail::usedFactFiles(*database);
  if (!used)
    return std::unexpected(used.error());
  return !used->empty();
}

} // namespace facts::commands
