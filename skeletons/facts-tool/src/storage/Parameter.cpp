#include "storage/Storage.h"

#include "storage/Sqlite.h"

#include <sqlite3.h>

#include <cstddef>
#include <cstdint>
#include <ranges>

namespace facts {
namespace {

SymbolId unpackSymbolId(std::uint64_t packed) {
  return {
      static_cast<FileId>(packed >> 32U),
      static_cast<std::uint32_t>(packed),
  };
}

} // namespace

std::expected<std::vector<Parameter>, std::error_code>
Storage::loadParameters(SymbolId id) {
  auto statement = storage::prepare(
      handle(),
      "SELECT name,type,line,col,offset,region_offset,region_size,flags,"
      "has_default FROM parameter WHERE symbol_id=?1 ORDER BY position");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id))) {
    return std::unexpected(storage::sqliteError(handle()));
  }

  std::vector<Parameter> parameters;
  auto step = sqlite3_step(statement->get());
  while (step == SQLITE_ROW) {
    parameters.push_back(Parameter{
        storage::columnText(statement->get(), 0),
        unpackSymbolId(sqlite3_column_int64(statement->get(), 1)),
        {static_cast<unsigned>(sqlite3_column_int64(statement->get(), 2)),
         static_cast<unsigned>(sqlite3_column_int64(statement->get(), 3)),
         static_cast<unsigned>(sqlite3_column_int64(statement->get(), 4))},
        {static_cast<unsigned>(sqlite3_column_int64(statement->get(), 5)),
         static_cast<unsigned>(sqlite3_column_int64(statement->get(), 6))},
        static_cast<std::uint8_t>(sqlite3_column_int64(statement->get(), 7)),
        sqlite3_column_int64(statement->get(), 8) != 0,
    });
    step = sqlite3_step(statement->get());
  }
  if (step != SQLITE_DONE) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  return parameters;
}

std::expected<void, std::error_code>
Storage::replaceParameters(SymbolId id, std::span<const Parameter> parameters) {
  auto clear =
      storage::prepare(handle(), "DELETE FROM parameter WHERE symbol_id=?1");
  if (!clear ||
      !storage::bindInteger(clear->get(), 1, storage::packSymbolId(id)) ||
      sqlite3_step(clear->get()) != SQLITE_DONE) {
    return std::unexpected(storage::sqliteError(handle()));
  }

  constexpr auto sql =
      "INSERT INTO parameter(symbol_id, position, name, type, line, col, "
      "offset, region_offset, region_size, flags, has_default) "
      "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)";
  for (const auto position :
       std::views::iota(std::size_t{0}, parameters.size())) {
    const auto &parameter = parameters[position];
    auto statement = storage::prepare(handle(), sql);
    if (!statement ||
        !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id)) ||
        !storage::bindInteger(statement->get(), 2, position) ||
        !storage::bindText(statement->get(), 3, parameter.name) ||
        !storage::bindInteger(statement->get(), 4,
                              storage::packSymbolId(parameter.type)) ||
        !storage::bindInteger(statement->get(), 5, parameter.loc.line) ||
        !storage::bindInteger(statement->get(), 6, parameter.loc.column) ||
        !storage::bindInteger(statement->get(), 7, parameter.loc.offset) ||
        !storage::bindInteger(statement->get(), 8, parameter.region.offset) ||
        !storage::bindInteger(statement->get(), 9, parameter.region.size) ||
        !storage::bindInteger(statement->get(), 10, parameter.flags) ||
        !storage::bindInteger(statement->get(), 11, parameter.hasDefault) ||
        sqlite3_step(statement->get()) != SQLITE_DONE) {
      return std::unexpected(storage::sqliteError(handle()));
    }
  }
  return {};
}

} // namespace facts
