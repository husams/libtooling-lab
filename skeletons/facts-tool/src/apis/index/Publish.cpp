#include "apis/index/Internal.h"

namespace facts::apis::index {
Result<RefreshResult> publish(Database &database, std::size_t sources,
                        std::size_t missing) {
  constexpr auto sql = R"sql(
CREATE TEMP TABLE api_symbol_next AS
  SELECT s.usr,s.qualified_name,s.kind,s.file_id,s.is_definition,s.path
  FROM temp.api_symbol_stage s JOIN file f ON f.id=s.file_id
  JOIN directory d ON d.id=f.directory_id JOIN component c ON c.id=d.component_id
  WHERE NOT EXISTS (SELECT 1 FROM api_index_invalidated_file invalid WHERE invalid.file_id=s.file_id)
  AND (s.is_definition=1 OR NOT EXISTS (
    SELECT 1 FROM temp.api_symbol_stage known JOIN file kf ON kf.id=known.file_id
    JOIN directory kd ON kd.id=kf.directory_id JOIN component kc ON kc.id=kd.component_id
    WHERE known.usr=s.usr AND known.is_definition=1 AND kc.repository_id IS c.repository_id
      AND NOT EXISTS (SELECT 1 FROM api_index_invalidated_file invalid WHERE invalid.file_id=known.file_id)))
  ORDER BY s.usr,s.file_id;
CREATE TEMP TABLE api_symbol_changed AS SELECT
 EXISTS(SELECT * FROM api_index_catalog EXCEPT SELECT * FROM temp.api_catalog_next)
 OR EXISTS(SELECT * FROM temp.api_catalog_next EXCEPT SELECT * FROM api_index_catalog)
 OR EXISTS(SELECT usr,qualified_name,kind,file_id,is_definition,path FROM global_symbol_index
  EXCEPT SELECT * FROM api_symbol_next)
 OR EXISTS(SELECT * FROM api_symbol_next EXCEPT
  SELECT usr,qualified_name,kind,file_id,is_definition,path FROM global_symbol_index) AS changed;
DELETE FROM global_symbol_index WHERE (SELECT changed FROM api_symbol_changed);
INSERT INTO global_symbol_index(usr,qualified_name,kind,file_id,is_definition,path)
 SELECT * FROM api_symbol_next WHERE (SELECT changed FROM api_symbol_changed) ORDER BY usr,file_id;
DELETE FROM api_index_catalog;
INSERT INTO api_index_catalog SELECT * FROM temp.api_catalog_next;
UPDATE global_symbol_index_state SET generation=generation+1
 WHERE id=1 AND (generation=0 OR (SELECT changed FROM api_symbol_changed));
)sql";
  return storage::execute(database.nativeHandle(), sql)
      .transform_error([&](auto) { return catalog::databaseError(database); })
      .and_then([&] { return updateIdentities(database); })
      .and_then([&] {
        return catalog::execute(database,
            "UPDATE global_symbol_index_state SET sources=?,missing_sources=? WHERE id=1",
            sources, missing);
      })
      .and_then([&] {
        return catalog::query(database,
            "SELECT generation,(SELECT count(*) FROM global_symbol_index) "
            "FROM global_symbol_index_state WHERE id=1",
            [&](const storage::Row &row) {
              return RefreshResult{row.integer(0), row.integer(1), sources, missing};
            });
      })
      .and_then([](auto rows) { return catalog::requireOne(std::move(rows), "symbol index state"); });
}
}
