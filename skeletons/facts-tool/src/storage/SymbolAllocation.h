#pragma once

#include "storage/ItlibGenerator.h"
#include "storage/StorageQuery.h"

#include <clang/AST/Type.h>
#include <limits>

namespace facts {

// BuiltinType::Kind + 1 is reserved even when no primitive row exists yet.
inline constexpr std::int64_t firstCompilerSymbolIndex =
    static_cast<std::int64_t>(clang::BuiltinType::LastKind) + 2;

inline std::expected<SymbolId, std::error_code>
allocateSymbolId(storage::Database &database, FileId file) {
  const auto sql =
      file == builtinFileId
          ? "INSERT INTO symbol_allocator(file_id,next_index) "
            "SELECT 0,MAX(?2,COALESCE(MAX(id)+1,0))+1 FROM symbol "
            "WHERE id BETWEEN 0 AND 4294967295 "
            "ON CONFLICT(file_id) DO UPDATE SET "
            "next_index=MAX(next_index+1,excluded.next_index) "
            "RETURNING next_index-1"
          : "INSERT INTO symbol_allocator(file_id,next_index) VALUES(?1,1) "
            "ON CONFLICT(file_id) DO UPDATE SET next_index=next_index+1 "
            "RETURNING next_index-1";
  auto read = [](const storage::Row &row) { return row.get<std::int64_t>(0); };
  auto rows = file == builtinFileId
                  ? database.query(sql, read, file, firstCompilerSymbolIndex)
                  : database.query(sql, read, file);
  return storage::detail::collectOne(
             storage::detail::toItlibGenerator(std::move(rows)))
      .and_then(
          [file](std::int64_t raw) -> std::expected<SymbolId, std::error_code> {
            if (raw < 0 || raw > std::numeric_limits<std::uint32_t>::max())
              return std::unexpected(
                  std::make_error_code(std::errc::value_too_large));
            return SymbolId{file, static_cast<std::uint32_t>(raw)};
          });
}

} // namespace facts
