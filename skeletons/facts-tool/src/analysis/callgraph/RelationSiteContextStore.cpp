#include "analysis/callgraph/RelationSiteContextStore.h"

#include "storage/SqliteDatabase.h"
#include "storage/StorageQuery.h"

#include <ranges>

namespace facts::callgraph {

bool validContext(const RelationSite &site) {
  if (!site.certainty)
    return !site.receiverType;
  if (*site.certainty == ReceiverCertainty::Exact)
    return site.receiverType.has_value();
  return *site.certainty == ReceiverCertainty::Possible && !site.receiverType;
}

std::expected<void, std::error_code>
insertRelationSites(storage::Database &database,
                    std::span<const RelationSite> sites) {
  if (!std::ranges::all_of(sites, validContext))
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  return database
      .executeBulk(
          "INSERT INTO relation_site(source_id,destination_id,kind,position,"
          "file_id,line,col,offset,receiver_type_id,certainty) "
          "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10) ON CONFLICT(source_id,"
          "destination_id,kind,position,file_id,offset) DO UPDATE SET "
          "receiver_type_id=CASE WHEN receiver_type_id IS excluded.receiver_"
          "type_id AND certainty IS excluded.certainty THEN receiver_type_id "
          "ELSE NULL END,certainty=CASE WHEN receiver_type_id IS excluded."
          "receiver_type_id AND certainty IS excluded.certainty THEN certainty "
          "ELSE ?11 END",
          sites, storage::detail::typedBinder([](auto bind,
                                                  const RelationSite &site) {
            return bind(site.source, site.destination, site.kind, site.position,
                        site.file, site.location.line, site.location.column,
                        site.location.offset, site.receiverType, site.certainty,
                        ReceiverCertainty::Possible);
          }))
      .transform([](const storage::BulkResult &) {});
}

} // namespace facts::callgraph
