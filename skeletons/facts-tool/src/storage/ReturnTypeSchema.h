#pragma once

namespace facts::storage {

inline constexpr auto returnTypeSchemaSql = R"sql(
CREATE TABLE IF NOT EXISTS callable_return_type (
  symbol_id INTEGER PRIMARY KEY REFERENCES symbol(id) ON DELETE CASCADE,
  canonical_type TEXT NOT NULL CHECK(canonical_type <> '')
);
)sql";

} // namespace facts::storage
