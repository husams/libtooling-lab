#include "storage/Storage.h"
#include "storage/StorageQuery.h"

#include <array>

namespace facts {

std::expected<void, std::error_code>
Storage::saveReturnType(SymbolId callable, const ReturnType &type) {
  const auto ensureTarget = [&]() -> std::expected<void, std::error_code> {
    if (type.target.file != builtinFileId)
      return {};
    if (type.target.index == 0 || type.builtinName.empty())
      return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    Symbol builtin{};
    builtin.id = type.target;
    builtin.usr = "c:@BT@" + std::to_string(type.target.index);
    builtin.qualifiedName = type.builtinName;
    builtin.flags |= bit(ImplicitBit) | bit(ExternalBit);
    return replaceSymbolRow(type.target, SymbolNode::Symbol, builtin);
  };
  const std::array relations{Relation{.source = callable,
                                      .destination = type.target,
                                      .kind = RelationKind::ReturnType}};
  return writeTransaction().and_then([&](OptionalTransaction transaction) {
    return ensureTarget()
        .and_then([&] {
          return database_.executeBulk(
              "DELETE FROM relation WHERE source_id=?1 AND kind=?2 "
              "AND destination_id<>?3",
              relations,
              storage::detail::typedBinder(
                  [](auto bind, const Relation &relation) {
                    return bind(relation.source, relation.kind,
                                relation.destination);
                  }));
        })
        .and_then([&](auto) { return addRelations(relations); })
        .and_then([&] {
          return database_.executeBulk(
              "INSERT INTO callable_return_type(symbol_id,canonical_type) "
              "VALUES(?1,?2) ON CONFLICT(symbol_id) DO UPDATE SET "
              "canonical_type=excluded.canonical_type",
              std::array{callable},
              storage::detail::typedBinder([&](auto bind, SymbolId id) {
                return bind(id, type.canonicalType);
              }));
        })
        .and_then([&](auto) { return commit(transaction); });
  });
}

} // namespace facts
