#include "apis/index/Internal.h"

namespace facts::apis::index {
namespace {
Result<void> copy(Database &target, Database &source, const std::string &path) {
  constexpr auto sql =
      "SELECT s.usr,s.qualified_name,coalesce(d.file_id,s.id>>32),"
      "s.kind,(d.file_id IS NOT NULL OR s.is_definition=1),coalesce(p.path,'') "
      "FROM symbol s LEFT JOIN definition d ON d.symbol_id=s.id "
      "LEFT JOIN facts_project_provenance p ON p.file_id=coalesce(d.file_id,s.id>>32)";
  return storage::prepare(target.nativeHandle(), stageInsert)
      .transform_error([&](auto) { return catalog::databaseError(target); })
      .and_then([&](storage::Statement insert) -> Result<void> {
        try {
          for (const auto &row : source.rows(sql)) {
            auto result = stage(target, insert, row.string(0), row.string(1),
                                row.integer(2), row.integer(3), row.integer(4), row.string(5), path);
            if (!result) return result;
          }
          return {};
        } catch (const storage::QueryError &error) {
          return std::unexpected(error.what());
        }
      });
}
}
Result<bool> copySource(Database &database, const std::filesystem::path &path) {
  std::error_code error;
  const bool exists = std::filesystem::exists(path, error);
  if (error) return std::unexpected("cannot inspect facts database: " + error.message());
  if (!exists) return false;
  if (!std::filesystem::is_regular_file(path, error) || error)
    return std::unexpected("facts database is not a regular file");
  return catalog::query(database,
      "SELECT EXISTS(SELECT 1 FROM temp.api_symbol_owner o WHERE o.facts_db=?1) "
      "AND NOT EXISTS(SELECT 1 FROM temp.api_symbol_owner o "
      "WHERE o.facts_db=?1 AND NOT EXISTS(SELECT 1 FROM api_index_invalidated_file invalid "
      "WHERE invalid.file_id=o.file_id))",
      [](const storage::Row &row) { return row.integer(0) != 0; }, path.string())
      .and_then([&](const auto &blocked) -> Result<bool> {
        if (blocked.at(0)) return true;
        return Database::open(path.string(), Database::readOnly)
            .transform_error([](auto error) { return error.message(); })
            .and_then([&](Database source) {
              return copy(database, source, path.string()).transform([] { return true; });
            });
      });
}
}
