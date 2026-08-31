#include "storage/Storage.h"

#include "analysis/callgraph/RelationSiteContextStore.h"
#include "storage/StorageQuery.h"

#include <algorithm>
#include <ranges>
#include <tuple>

namespace facts {
namespace {

auto relationKey(const Relation &relation) {
  return std::tuple{relation.source, relation.destination, relation.kind,
                    relation.position};
}

auto relationKey(const RelationSite &site) {
  return std::tuple{site.source, site.destination, site.kind, site.position};
}

bool validUseRelation(const Relation &relation) {
  return relation.kind == RelationKind::Uses && relation.source != SymbolId{} &&
         relation.destination != SymbolId{};
}

bool validUseSite(const RelationSite &site) {
  return site.kind == RelationKind::Uses && site.source != SymbolId{} &&
         site.destination != SymbolId{} && site.file != builtinFileId &&
         site.location.line != 0 && site.location.column != 0;
}

bool validRelationSite(const RelationSite &site) {
  const auto siteBacked = site.kind == RelationKind::Uses ||
                          site.kind == RelationKind::Calls ||
                          site.kind == RelationKind::DispatchCalls ||
                          site.kind == RelationKind::Overrides;
  return siteBacked && site.source != SymbolId{} &&
         site.destination != SymbolId{} && site.file != builtinFileId &&
         site.location.line != 0 && site.location.column != 0 &&
         callgraph::validContext(site);
}

bool sitesBelongToRelations(std::span<const Relation> relations,
                            std::span<const RelationSite> sites) {
  return std::ranges::all_of(sites, [&](const RelationSite &site) {
    return std::ranges::any_of(relations, [&](const Relation &relation) {
      return relationKey(relation) == relationKey(site);
    });
  });
}

std::expected<void, std::error_code>
validateUseFacts(std::span<const Relation> relations,
                 std::span<const RelationSite> sites) {
  if (!std::ranges::all_of(relations, validUseRelation) ||
      !std::ranges::all_of(sites, validUseSite) ||
      !sitesBelongToRelations(relations, sites)) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  return {};
}

} // namespace

std::expected<void, std::error_code>
Storage::addRelationSites(std::span<const RelationSite> sites) {
  if (!std::ranges::all_of(sites, validRelationSite)) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  return callgraph::insertRelationSites(database_, sites);
}

std::expected<void, std::error_code>
Storage::recomputeRelationCounts(std::span<const Relation> relations) {
  return database_
      .executeBulk(
          "UPDATE relation SET count=(SELECT COUNT(*) FROM relation_site site "
          "WHERE site.source_id=relation.source_id AND "
          "site.destination_id=relation.destination_id AND "
          "site.kind=relation.kind AND site.position=relation.position) "
          "WHERE source_id=?1 AND destination_id=?2 AND kind=?3 AND "
          "position=?4",
          relations,
          storage::detail::typedBinder([](auto bind, const Relation &relation) {
            return bind(relation.source, relation.destination, relation.kind,
                        relation.position);
          }))
      .transform([](const storage::BulkResult &) {});
}

std::expected<void, std::error_code>
Storage::addUseFacts(std::span<const Relation> relations,
                     std::span<const RelationSite> sites) {
  return validateUseFacts(relations, sites).and_then([&] {
    return addRelationFacts(relations, sites);
  });
}

std::expected<void, std::error_code>
Storage::addRelationFacts(std::span<const Relation> relations,
                          std::span<const RelationSite> sites) {
  auto transaction = writeTransaction();
  if (!transaction) {
    return std::unexpected(transaction.error());
  }

  if (!std::ranges::all_of(sites, validRelationSite) ||
      !sitesBelongToRelations(relations, sites)) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  return addRelations(relations)
      .and_then([&] { return addRelationSites(sites); })
      .and_then([&] { return recomputeRelationCounts(relations); })
      .and_then([&] { return commit(*transaction); });
}

} // namespace facts
