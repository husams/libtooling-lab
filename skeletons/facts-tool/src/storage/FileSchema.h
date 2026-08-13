#ifndef FACTS_TOOL_STORAGE_FILE_SCHEMA_H
#define FACTS_TOOL_STORAGE_FILE_SCHEMA_H

namespace facts {

inline constexpr const char *fileSchemaSql = R"sql(

CREATE TABLE IF NOT EXISTS file (
  id   INTEGER PRIMARY KEY CHECK(id >= 1),
  path TEXT NOT NULL UNIQUE
);

)sql";

} // namespace facts

#endif // FACTS_TOOL_STORAGE_FILE_SCHEMA_H
