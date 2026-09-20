#pragma once

namespace facts::apis::index {
inline constexpr auto symbolTableSql = R"sql(
CREATE TABLE IF NOT EXISTS global_symbol_index (
  position INTEGER PRIMARY KEY,
  usr TEXT NOT NULL CHECK(usr<>''),
  qualified_name TEXT NOT NULL,
  kind TEXT NOT NULL,
  path TEXT NOT NULL,
  file_id INTEGER NOT NULL REFERENCES file(id) ON DELETE CASCADE,
  is_definition INTEGER NOT NULL CHECK(is_definition IN (0,1)),
  UNIQUE(usr,file_id)
);
)sql";
inline constexpr auto symbolLookupSql =
    "CREATE INDEX IF NOT EXISTS global_symbol_name "
    "ON global_symbol_index(qualified_name,position,kind,usr,file_id)";
}
