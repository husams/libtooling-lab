#include "storage/SymbolIdentity.h"

#include "storage/Sqlite.h"

#include <sqlite3.h>

#include <cstdint>
#include <limits>
#include <optional>

namespace facts {
namespace {

std::expected<std::optional<std::uint32_t>, std::error_code>
findSymbolIndex(sqlite3 *database, FileId file, std::string_view identity) {
  auto statement = storage::prepare(
      database,
      "SELECT file_index FROM symbol WHERE file_id=?1 AND identity=?2");
  if (!statement || !storage::bindInteger(statement->get(), 1, file) ||
      !storage::bindText(statement->get(), 2, identity)) {
    return std::unexpected(storage::sqliteError(database));
  }

  const auto step = sqlite3_step(statement->get());
  if (step == SQLITE_DONE) {
    return std::optional<std::uint32_t>{};
  }
  if (step != SQLITE_ROW) {
    return std::unexpected(storage::sqliteError(database));
  }

  const auto raw = sqlite3_column_int64(statement->get(), 0);
  if (raw < 0 || raw > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected(std::make_error_code(std::errc::value_too_large));
  }
  return std::optional{static_cast<std::uint32_t>(raw)};
}

std::expected<std::uint32_t, std::error_code>
allocateSymbolIndex(sqlite3 *database, FileId file) {
  auto statement = storage::prepare(
      database, "INSERT INTO symbol_allocator(file_id,next_index) VALUES(?1,1) "
                "ON CONFLICT(file_id) DO UPDATE SET next_index=next_index+1 "
                "RETURNING next_index-1");
  if (!statement || !storage::bindInteger(statement->get(), 1, file) ||
      sqlite3_step(statement->get()) != SQLITE_ROW) {
    return std::unexpected(storage::sqliteError(database));
  }

  const auto raw = sqlite3_column_int64(statement->get(), 0);
  if (raw < 0 || raw > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected(std::make_error_code(std::errc::value_too_large));
  }
  return static_cast<std::uint32_t>(raw);
}

} // namespace

std::string symbolIdentity(const Symbol &symbol) {
  if (!symbol.usr.empty()) {
    return "usr:" + symbol.usr;
  }
  return "decl:" + symbol.qualifiedName + ':' +
         std::to_string(static_cast<unsigned>(symbol.Kind)) + ':' +
         std::to_string(symbol.loc.line) + ':' +
         std::to_string(symbol.loc.column);
}

std::expected<SymbolId, std::error_code>
findOrAllocateSymbol(sqlite3 *database, FileId file,
                     std::string_view identity) {
  return findSymbolIndex(database, file, identity)
      .and_then([database, file](std::optional<std::uint32_t> index)
                    -> std::expected<std::uint32_t, std::error_code> {
        if (index) {
          return *index;
        }
        return allocateSymbolIndex(database, file);
      })
      .transform([file](std::uint32_t index) { return SymbolId{file, index}; });
}

} // namespace facts
