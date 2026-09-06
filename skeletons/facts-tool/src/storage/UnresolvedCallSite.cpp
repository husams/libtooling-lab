#include "storage/Storage.h"

#include "storage/StorageQuery.h"

#include <ranges>

namespace facts {

std::expected<void, std::error_code>
Storage::addUnresolvedCallSites(std::span<const UnresolvedCallSite> sites) {
  if (!std::ranges::all_of(sites, [](const auto &site) {
        return site.source != SymbolId{} && site.file != builtinFileId &&
               site.location.line != 0 && site.location.column != 0;
      }))
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  auto transaction = writeTransaction();
  if (!transaction)
    return std::unexpected(transaction.error());
  return database_
      .executeBulk(
          "INSERT INTO callgraph_unresolved_site(source_id,file_id,"
          "offset,line,col) VALUES(?1,?2,?3,?4,?5) "
          "ON CONFLICT(source_id,file_id,offset) DO NOTHING",
          sites, storage::detail::typedBinder([](auto bind, const auto &site) {
            return bind(site.source, site.file, site.location.offset,
                        site.location.line, site.location.column);
          }))
      .and_then(
          [&](const storage::BulkResult &) { return commit(*transaction); });
}

std::expected<void, std::error_code>
Storage::clearUnresolvedCallSites(std::span<const SymbolId> callers) {
  auto transaction = writeTransaction();
  if (!transaction)
    return std::unexpected(transaction.error());
  return database_
      .executeBulk("DELETE FROM callgraph_unresolved_site WHERE source_id=?1",
                   callers,
                   storage::detail::typedBinder(
                       [](auto bind, const auto &id) { return bind(id); }))
      .and_then(
          [&](const storage::BulkResult &) { return commit(*transaction); });
}

std::expected<void, std::error_code>
Storage::clearCallGraphFacts(std::span<const SymbolId> callers) {
  auto transaction = writeTransaction();
  if (!transaction)
    return std::unexpected(transaction.error());
  auto sites = database_.executeBulk(
      "DELETE FROM relation_site WHERE source_id=?1 AND kind IN (?2,?3)",
      callers, storage::detail::typedBinder([](auto bind, const auto &id) {
        return bind(id, RelationKind::Calls, RelationKind::DispatchCalls);
      }));
  if (!sites)
    return std::unexpected(sites.error());
  auto relations = database_.executeBulk(
      "DELETE FROM relation WHERE source_id=?1 AND kind IN (?2,?3)", callers,
      storage::detail::typedBinder([](auto bind, const auto &id) {
        return bind(id, RelationKind::Calls, RelationKind::DispatchCalls);
      }));
  if (!relations)
    return std::unexpected(relations.error());
  auto unresolved = clearUnresolvedCallSites(callers);
  if (!unresolved)
    return std::unexpected(unresolved.error());
  return invalidateCallGraphEntries(callers).and_then(
      [&] { return commit(*transaction); });
}

} // namespace facts
