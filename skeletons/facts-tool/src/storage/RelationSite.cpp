#include "storage/Storage.h"

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
  if (!std::ranges::all_of(sites, validUseSite)) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  return database_
      .executeBulk(
          "INSERT INTO relation_site(source_id,destination_id,kind,position,"
          "file_id,line,col,offset) VALUES(?1,?2,?3,?4,?5,?6,?7,?8) "
          "ON CONFLICT DO NOTHING",
          sites,
          storage::detail::typedBinder([](auto bind, const RelationSite &site) {
            return bind(site.source, site.destination, site.kind, site.position,
                        site.file, site.location.line, site.location.column,
                        site.location.offset);
          }))
      .transform([](const storage::BulkResult &) {});
}

std::expected<void, std::error_code>
Storage::recomputeUseCounts(std::span<const Relation> relations) {
  return database_
      .executeBulk(
          "UPDATE relation SET count=(SELECT COUNT(*) FROM relation_site site "
          "WHERE site.source_id=relation.source_id AND "
          "site.destination_id=relation.destination_id AND "
          "site.kind=relation.kind AND site.position=relation.position) "
          "WHERE source_id=?1 AND destination_id=?2 AND kind=?3 AND "
          "position=?4 AND kind=?5",
          relations,
          storage::detail::typedBinder([](auto bind, const Relation &relation) {
            return bind(relation.source, relation.destination, relation.kind,
                        relation.position, RelationKind::Uses);
          }))
      .transform([](const storage::BulkResult &) {});
}

std::expected<void, std::error_code>
Storage::addUseFacts(std::span<const Relation> relations,
                     std::span<const RelationSite> sites) {
  auto transaction = writeTransaction();
  if (!transaction) {
    return std::unexpected(transaction.error());
  }

  return validateUseFacts(relations, sites)
      .and_then([&] { return addRelations(relations); })
      .and_then([&] { return addRelationSites(sites); })
      .and_then([&] { return recomputeUseCounts(relations); })
      .and_then([&] { return commit(*transaction); });
}

} // namespace facts
