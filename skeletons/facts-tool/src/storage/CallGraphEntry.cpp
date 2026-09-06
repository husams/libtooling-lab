#include "storage/Storage.h"

#include "storage/ItlibGenerator.h"
#include "storage/StorageQuery.h"

#include <array>
#include <ranges>

namespace facts {

std::expected<void, std::error_code>
Storage::addCallGraphEntries(std::span<const CallGraphEntry> entries) {
  if (!std::ranges::all_of(entries, [](const auto &entry) {
        return entry.symbolId != SymbolId{} &&
               entry.symbolId == entry.graphNodeRef;
      }))
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  auto transaction = writeTransaction();
  if (!transaction) return std::unexpected(transaction.error());
  return database_
      .executeBulk("INSERT INTO callgraph_entry(symbol_id,graph_node_ref) "
                   "VALUES(?1,?2) ON CONFLICT(symbol_id) DO UPDATE SET "
                   "graph_node_ref=excluded.graph_node_ref",
                   entries, storage::detail::typedBinder([](auto bind,
                                                             const auto &entry) {
                     return bind(entry.symbolId, entry.graphNodeRef);
                   }))
      .and_then([&](const storage::BulkResult &) { return commit(*transaction); });
}

std::expected<void, std::error_code>
Storage::invalidateCallGraphEntries(std::span<const SymbolId> symbols) {
  auto transaction = writeTransaction();
  if (!transaction) return std::unexpected(transaction.error());
  return database_
      .executeBulk("DELETE FROM callgraph_entry WHERE symbol_id=?1", symbols,
                   storage::detail::typedBinder([](auto bind, const auto &id) {
                     return bind(id);
                   }))
      .and_then([&](const storage::BulkResult &) { return commit(*transaction); });
}

std::expected<void, std::error_code>
Storage::invalidateCallGraphEntries(FileId file) {
  const std::array files{file};
  return invalidateCallGraphEntries(std::span<const FileId>{files});
}

std::expected<void, std::error_code>
Storage::invalidateCallGraphEntries(std::span<const FileId> files) {
  auto transaction = writeTransaction();
  if (!transaction) return std::unexpected(transaction.error());
  return database_
      .executeBulk("DELETE FROM callgraph_entry WHERE (symbol_id >> 32)=?1",
                   files, storage::detail::typedBinder([](auto bind, const auto &id) {
                     return bind(id);
                   }))
      .and_then([&](const storage::BulkResult &) { return commit(*transaction); });
}

std::expected<std::optional<CallGraphEntry>, std::error_code>
Storage::findCallGraphEntry(SymbolId symbol) {
  auto transaction = readTransaction();
  if (!transaction) return std::unexpected(transaction.error());
  auto values = storage::detail::toItlibGenerator(database_.query(
      "SELECT symbol_id,graph_node_ref FROM callgraph_entry WHERE symbol_id=?1",
      [](const storage::Row &row) {
        return CallGraphEntry{row.get<SymbolId>(0), row.get<SymbolId>(1)};
      },
      symbol));
  return storage::detail::collectOptional(std::move(values)).and_then(
      [&](auto entry) { return commit(*transaction).transform([&] { return entry; }); });
}

} // namespace facts
