#include "storage/Storage.h"

#include "storage/Initializer.h"
#include "storage/ItlibGenerator.h"
#include "storage/SemanticProperties.h"
#include "storage/StorageQuery.h"

#include <cstddef>
#include <cstdint>
#include <ranges>

namespace facts {

std::expected<std::vector<Parameter>, std::error_code>
Storage::loadParameters(SymbolId id) {
  auto rows = storage::detail::toItlibGenerator(database_.query(
      "SELECT p.name,p.type,p.line,p.col,p.offset,p.region_offset,"
      "p.region_size,p.is_pointer,p.is_lvalue_reference,"
      "p.is_rvalue_reference,p.is_forwarding_reference,p.is_const,p.is_pack,"
      "p.has_default,d.expression,d.evaluated_kind,d.evaluated_value "
      "FROM parameter p LEFT JOIN parameter_default d "
      "ON d.symbol_id=p.symbol_id AND d.position=p.position "
      "WHERE p.symbol_id=?1 ORDER BY p.position",
      [](const storage::Row &row) {
        return Parameter{
            .name = row.get<std::string>(0),
            .type = row.get<SymbolId>(1),
            .loc = {row.get<unsigned>(2), row.get<unsigned>(3),
                    row.get<unsigned>(4)},
            .region = {row.get<unsigned>(5), row.get<unsigned>(6)},
            .flags = storage::parameterFlags({
                .isPointer = row.get<bool>(7),
                .isLValueReference = row.get<bool>(8),
                .isRValueReference = row.get<bool>(9),
                .isForwardingReference = row.get<bool>(10),
                .isConst = row.get<bool>(11),
                .isPack = row.get<bool>(12),
            }),
            .hasDefault = row.get<bool>(13),
            .defaultValue = storage::loadInitializer(row, 14, 15, 16),
        };
      },
      id));
  return storage::detail::collectGenerator(std::move(rows));
}

std::expected<void, std::error_code>
Storage::replaceParameters(SymbolId id, std::span<const Parameter> parameters) {
  const std::array trimRows{parameters.size()};
  auto trimmed = database_.executeBulk(
      "DELETE FROM parameter WHERE symbol_id=?1 AND position>=?2", trimRows,
      [id](sqlite3_stmt *statement, std::size_t size) {
        return storage::bindParameters(statement, id, size);
      });
  if (!trimmed) {
    return std::unexpected(trimmed.error());
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
  auto positions = std::views::iota(std::size_t{0}, parameters.size());
  auto saved = database_.executeBulk(
      sql, positions,
      [id, parameters](sqlite3_stmt *statement, std::size_t position) {
        const auto &parameter = parameters[position];
        const auto properties = storage::parameterProperties(parameter.flags);
        return storage::bindParameters(
            statement, id, position, parameter.name, parameter.type,
            parameter.loc.line, parameter.loc.column, parameter.loc.offset,
            parameter.region.offset, parameter.region.size,
            properties.isPointer, properties.isLValueReference,
            properties.isRValueReference, properties.isForwardingReference,
            properties.isConst, properties.isPack, parameter.hasDefault);
      });
  if (!saved) {
    return std::unexpected(saved.error());
  }

  auto defaultPositions = positions | std::views::filter([parameters](auto i) {
                            return parameters[i].defaultValue.has_value();
                          });
  auto defaults = database_.executeBulk(
      "INSERT INTO parameter_default(symbol_id,position,expression,"
      "evaluated_kind,evaluated_value) VALUES(?1,?2,?3,?4,?5) "
      "ON CONFLICT(symbol_id,position) DO UPDATE SET "
      "expression=excluded.expression,evaluated_kind=excluded.evaluated_kind,"
      "evaluated_value=excluded.evaluated_value",
      defaultPositions,
      [id, parameters](sqlite3_stmt *statement, std::size_t position) {
        const auto value =
            storage::initializerColumns(*parameters[position].defaultValue);
        return storage::bindParameters(
            statement, id, position, value.expression, value.kind, value.value);
      });
  if (!defaults) {
    return std::unexpected(defaults.error());
  }
  return {};
}

} // namespace facts
