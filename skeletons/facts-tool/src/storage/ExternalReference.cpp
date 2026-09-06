#include "storage/Storage.h"

#include "storage/ItlibGenerator.h"
#include "storage/StorageQuery.h"

#include <ranges>

namespace facts {
namespace {
bool validExternal(const ExternalReference &value) {
  return value.source != SymbolId{} && value.destination != SymbolId{} &&
         value.destination == value.externalSymbol &&
         value.file != builtinFileId;
}
} // namespace

std::expected<bool, std::error_code> Storage::isExternal(SymbolId symbol) {
  auto transaction = readTransaction();
  if (!transaction) return std::unexpected(transaction.error());
  auto values = storage::detail::toItlibGenerator(database_.query(
      "SELECT is_external OR NOT EXISTS(SELECT 1 FROM definition WHERE "
      "symbol_id=symbol.id) FROM symbol WHERE id=?1",
      [](const storage::Row &row) { return row.get<bool>(0); }, symbol));
  return storage::detail::collectOne(std::move(values)).and_then(
      [&](bool value) { return commit(*transaction).transform([&] { return value; }); });
}

std::expected<void, std::error_code> Storage::addCallGraphExternalReferences(
    std::span<const ExternalReference> references) {
  if (!std::ranges::all_of(references, validExternal))
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  auto transaction = writeTransaction();
  if (!transaction) return std::unexpected(transaction.error());
  return database_
      .executeBulk("INSERT INTO callgraph_external_reference(source_id,destination_id,"
                   "kind,position,file_id,offset,external_symbol_id) VALUES(?1,?2,?3,?4,?5,?6,?7)"
                   " ON CONFLICT(source_id,destination_id,kind,position,file_id,offset) DO NOTHING",
                   references, storage::detail::typedBinder([](auto bind, const auto &value) {
                     return bind(value.source, value.destination, value.kind, value.position,
                                 value.file, value.offset, value.externalSymbol);
                   }))
      .and_then([&](const storage::BulkResult &) { return commit(*transaction); });
}

std::expected<void, std::error_code> Storage::addCallGraphFacts(
    std::span<const Relation> relations, std::span<const RelationSite> sites,
    std::span<const ExternalReference> references,
    std::span<const CallGraphEntry> entries,
    std::span<const UnresolvedCallSite> unresolved) {
  if (!std::ranges::all_of(references, validExternal) ||
      !std::ranges::all_of(entries, [](const auto &entry) {
        return entry.symbolId != SymbolId{} && entry.symbolId == entry.graphNodeRef;
      }))
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  auto transaction = writeTransaction();
  if (!transaction) return std::unexpected(transaction.error());
  const auto callers = entries |
                       std::views::transform([](const auto &entry) {
                         return entry.symbolId;
                       }) |
                       std::ranges::to<std::vector>();
  if (auto cleared = clearCallGraphFacts(callers); !cleared)
    return std::unexpected(cleared.error());
  return addRelationFacts(relations, sites)
      .and_then([&] { return addCallGraphExternalReferences(references); })
      .and_then([&] { return addCallGraphEntries(entries); })
      .and_then([&] { return addUnresolvedCallSites(unresolved); })
      .and_then([&] { return commit(*transaction); });
}

} // namespace facts
