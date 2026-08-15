#include "storage/Storage.h"

#include "storage/Initializer.h"
#include "storage/SemanticProperties.h"
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

std::expected<void, std::error_code>
storeParameterDefault(sqlite3 *database, SymbolId id, std::size_t position,
                      const std::optional<Initializer> &defaultValue) {
  if (!defaultValue) {
    return {};
  }

  auto statement = storage::prepare(
      database,
      "INSERT INTO parameter_default(symbol_id,position,expression,"
      "evaluated_kind,evaluated_value) VALUES(?1,?2,?3,?4,?5) "
      "ON CONFLICT(symbol_id,position) DO UPDATE SET "
      "expression=excluded.expression,evaluated_kind=excluded.evaluated_kind,"
      "evaluated_value=excluded.evaluated_value");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id)) ||
      !storage::bindInteger(statement->get(), 2, position) ||
      !storage::bindInitializer(statement->get(), 3, 4, 5, *defaultValue) ||
      sqlite3_step(statement->get()) != SQLITE_DONE) {
    return std::unexpected(storage::sqliteError(database));
  }
  return {};
}

} // namespace

std::expected<std::vector<Parameter>, std::error_code>
Storage::loadParameters(SymbolId id) {
  auto statement = storage::prepare(
      handle(),
      "SELECT p.name,p.type,p.line,p.col,p.offset,p.region_offset,"
      "p.region_size,p.is_pointer,p.is_lvalue_reference,"
      "p.is_rvalue_reference,p.is_forwarding_reference,p.is_const,p.is_pack,"
      "p.has_default,d.expression,d.evaluated_kind,d.evaluated_value "
      "FROM parameter p LEFT JOIN parameter_default d "
      "ON d.symbol_id=p.symbol_id AND d.position=p.position "
      "WHERE p.symbol_id=?1 ORDER BY p.position");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id))) {
    return std::unexpected(storage::sqliteError(handle()));
  }

  std::vector<Parameter> parameters;
  auto step = sqlite3_step(statement->get());
  while (step == SQLITE_ROW) {
    parameters.push_back(Parameter{
        .name = storage::columnText(statement->get(), 0),
        .type = unpackSymbolId(sqlite3_column_int64(statement->get(), 1)),
        .loc =
            {static_cast<unsigned>(sqlite3_column_int64(statement->get(), 2)),
             static_cast<unsigned>(sqlite3_column_int64(statement->get(), 3)),
             static_cast<unsigned>(sqlite3_column_int64(statement->get(), 4))},
        .region =
            {static_cast<unsigned>(sqlite3_column_int64(statement->get(), 5)),
             static_cast<unsigned>(sqlite3_column_int64(statement->get(), 6))},
        .flags = storage::parameterFlags({
            .isPointer = sqlite3_column_int64(statement->get(), 7) != 0,
            .isLValueReference = sqlite3_column_int64(statement->get(), 8) != 0,
            .isRValueReference = sqlite3_column_int64(statement->get(), 9) != 0,
            .isForwardingReference =
                sqlite3_column_int64(statement->get(), 10) != 0,
            .isConst = sqlite3_column_int64(statement->get(), 11) != 0,
            .isPack = sqlite3_column_int64(statement->get(), 12) != 0,
        }),
        .hasDefault = sqlite3_column_int64(statement->get(), 13) != 0,
        .defaultValue = storage::loadInitializer(statement->get(), 14, 15, 16),
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
  auto trim = storage::prepare(
      handle(), "DELETE FROM parameter WHERE symbol_id=?1 AND position>=?2");
  if (!trim ||
      !storage::bindInteger(trim->get(), 1, storage::packSymbolId(id)) ||
      !storage::bindInteger(trim->get(), 2, parameters.size()) ||
      sqlite3_step(trim->get()) != SQLITE_DONE) {
    return std::unexpected(storage::sqliteError(handle()));
  }

  constexpr auto sql =
      "INSERT INTO parameter(symbol_id, position, name, type, line, col, "
      "offset, region_offset, region_size, is_pointer, is_lvalue_reference, "
      "is_rvalue_reference, is_forwarding_reference, is_const, is_pack, "
      "has_default) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,"
      "?14,?15,?16) ON CONFLICT(symbol_id,position) DO UPDATE SET "
      "name=excluded.name,type=excluded.type,line=excluded.line,col=excluded."
      "col,"
      "offset=excluded.offset,region_offset=excluded.region_offset,"
      "region_size=excluded.region_size,is_pointer=excluded.is_pointer,"
      "is_lvalue_reference=excluded.is_lvalue_reference,"
      "is_rvalue_reference=excluded.is_rvalue_reference,"
      "is_forwarding_reference=excluded.is_forwarding_reference,"
      "is_const=excluded.is_const,is_pack=excluded.is_pack,"
      "has_default=MAX(parameter.has_default,excluded.has_default)";
  for (const auto position :
       std::views::iota(std::size_t{0}, parameters.size())) {
    const auto &parameter = parameters[position];
    const auto properties = storage::parameterProperties(parameter.flags);
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
        !storage::bindInteger(statement->get(), 10, properties.isPointer) ||
        !storage::bindInteger(statement->get(), 11,
                              properties.isLValueReference) ||
        !storage::bindInteger(statement->get(), 12,
                              properties.isRValueReference) ||
        !storage::bindInteger(statement->get(), 13,
                              properties.isForwardingReference) ||
        !storage::bindInteger(statement->get(), 14, properties.isConst) ||
        !storage::bindInteger(statement->get(), 15, properties.isPack) ||
        !storage::bindInteger(statement->get(), 16, parameter.hasDefault) ||
        sqlite3_step(statement->get()) != SQLITE_DONE) {
      return std::unexpected(storage::sqliteError(handle()));
    }
    const auto storedDefault =
        storeParameterDefault(handle(), id, position, parameter.defaultValue);
    if (!storedDefault) {
      return storedDefault;
    }
  }
  return {};
}

} // namespace facts
