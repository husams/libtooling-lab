#include "storage/Storage.h"

#include "storage/StorageQuery.h"

#include <algorithm>
#include <ranges>
#include <tuple>

namespace facts {
namespace {

bool validPointerCall(const PointerCallSite &site) {
  return site.source != SymbolId{} && site.file != builtinFileId &&
         site.location.line != 0 && site.location.column != 0 &&
         (!site.target || *site.target != SymbolId{}) &&
         !site.signature.empty() && !site.expression.empty();
}

auto latestPointerSites(std::span<const PointerCallSite> sites) {
  const auto key = [](const PointerCallSite *site) {
    return std::tuple{site->source, site->file, site->location.offset};
  };
  // Keep the last observation for each persisted site key. The typed row and
  // its optional value relation must describe the same observation.
  auto latest = sites | std::views::reverse |
                std::views::transform([](const auto &site) { return &site; }) |
                std::ranges::to<std::vector>();
  std::ranges::stable_sort(latest, {}, key);
  latest.erase(std::ranges::unique(latest, {}, key).begin(), latest.end());
  return latest;
}

Relation pointerRelation(const PointerCallSite &site) {
  return {.source = site.source,
          .destination = *site.target,
          .kind = RelationKind::PointerCalls};
}

RelationSite pointerRelationSite(const PointerCallSite &site) {
  return {.source = site.source,
          .destination = *site.target,
          .kind = RelationKind::PointerCalls,
          .file = site.file,
          .location = site.location};
}

} // namespace

std::expected<void, std::error_code>
Storage::addPointerCallSites(std::span<const PointerCallSite> observations) {
  if (observations.empty())
    return {};
  if (!std::ranges::all_of(observations, validPointerCall))
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  auto transaction = writeTransaction();
  if (!transaction)
    return std::unexpected(transaction.error());
  const auto latest = latestPointerSites(observations);
  auto sites = latest | std::views::transform([](const auto *site)
                                                -> const PointerCallSite & {
                 return *site;
               });
  auto callers = sites | std::views::transform(&PointerCallSite::source) |
                 std::ranges::to<std::vector>();
  std::ranges::sort(callers);
  callers.erase(std::ranges::unique(callers).begin(), callers.end());
  auto known = sites | std::views::filter([](const auto &site) {
                       return site.target.has_value();
                     });
  const auto relations = known | std::views::transform(pointerRelation) |
                         std::ranges::to<std::vector>();
  const auto relationSites = known | std::views::transform(pointerRelationSite) |
                             std::ranges::to<std::vector>();
  return database_
      .executeBulk(
          "DELETE FROM relation_site WHERE source_id=?1 AND destination_id="
          "(SELECT target_id FROM callgraph_pointer_call_site WHERE "
          "source_id=?1 AND file_id=?2 AND offset=?3) AND kind=24 AND "
          "position=0 AND file_id=?2 AND offset=?3",
          sites, storage::detail::typedBinder([](auto bind, const auto &site) {
            return bind(site.source, site.file, site.location.offset);
          }))
      .and_then([&](auto) {
        return database_.executeBulk(
            "INSERT INTO callgraph_pointer_call_site(source_id,target_id,"
            "file_id,offset,line,col,signature,expression) "
            "VALUES(?1,?2,?3,?4,?5,?6,?7,?8) "
            "ON CONFLICT(source_id,file_id,offset) DO UPDATE SET "
            "target_id=excluded.target_id,line=excluded.line,col=excluded.col,"
            "signature=excluded.signature,expression=excluded.expression",
            sites,
            storage::detail::typedBinder([](auto bind, const auto &site) {
              return bind(site.source, site.target, site.file,
                          site.location.offset, site.location.line,
                          site.location.column, site.signature,
                          site.expression);
            }));
      })
      .and_then([&](auto) { return addRelations(relations); })
      .and_then([&] { return addRelationSites(relationSites); })
      .and_then([&] {
        // Replacing an invocation may remove its previous value declaration.
        // Keep occurrence counts and empty relation cleanup in the same write.
        return database_.executeBulk(
            "UPDATE relation SET count=(SELECT COUNT(*) FROM relation_site s "
            "WHERE s.source_id=relation.source_id AND "
            "s.destination_id=relation.destination_id AND s.kind=24 AND "
            "s.position=relation.position) WHERE source_id=?1 AND kind=24",
            callers, storage::detail::typedBinder([](auto bind, const auto &id) {
              return bind(id);
            }));
      })
      .and_then([&](auto) {
        return database_.executeBulk(
            "DELETE FROM relation WHERE source_id=?1 AND kind=24 AND count=0",
            callers, storage::detail::typedBinder([](auto bind, const auto &id) {
              return bind(id);
            }));
      })
      .and_then([&](auto) { return commit(*transaction); });
}

std::expected<void, std::error_code>
Storage::clearPointerCallSites(std::span<const SymbolId> callers) {
  auto transaction = writeTransaction();
  if (!transaction)
    return std::unexpected(transaction.error());
  return database_
      .executeBulk("DELETE FROM relation WHERE source_id=?1 AND kind=24",
                   callers,
                   storage::detail::typedBinder(
                       [](auto bind, const auto &id) { return bind(id); }))
      .and_then([&](auto) {
        return database_.executeBulk(
            "DELETE FROM callgraph_pointer_call_site WHERE source_id=?1",
            callers,
            storage::detail::typedBinder(
                [](auto bind, const auto &id) { return bind(id); }));
      })
      .and_then([&](auto) { return commit(*transaction); });
}

} // namespace facts
