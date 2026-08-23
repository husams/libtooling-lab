#include "storage/Storage.h"

#include "storage/SemanticProperties.h"
#include "storage/StorageQuery.h"
#include <system_error>

namespace facts {

std::expected<void, std::error_code>
Storage::addRelations(std::span<const Relation> relations) {
  return database_
      .executeBulk(
          "INSERT INTO relation(source_id,destination_id,kind,position,access,"
          "is_virtual_base,is_implicit,is_lexical,count) "
          "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9) ON CONFLICT(source_id,"
          "destination_id,kind,position) DO NOTHING",
          relations,
          storage::detail::typedBinder([](auto bind, const Relation &relation) {
            const auto properties = storage::relationProperties(relation.flags);
            return bind(relation.source, relation.destination, relation.kind,
                        relation.position, properties.access,
                        properties.isVirtualBase, properties.isImplicit,
                        properties.isLexical, relation.count);
          }))
      .transform([](const storage::BulkResult &) {});
}

} // namespace facts
