#include "storage/Storage.h"

#include "storage/ItlibGenerator.h"
#include "storage/StorageQuery.h"

#include <array>

namespace facts {

std::expected<std::optional<Storage::DefinitionFacts>, std::error_code>
Storage::loadDefinition(SymbolId id) {
  auto definitions = storage::detail::toItlibGenerator(database_.query(
      "SELECT file_id,offset,size FROM definition WHERE symbol_id=?1",
      [](const storage::Row &row) {
        return DefinitionFacts{
            .file = row.get<FileId>(0),
            .region = Region{row.get<unsigned>(1), row.get<unsigned>(2)}};
      },
      id));
  return storage::detail::collectOptional(std::move(definitions));
}

std::expected<void, std::error_code>
Storage::replaceDefinition(SymbolId id, FileId file,
                           const std::optional<Region> &definition) {
  if (!definition) {
    return {};
  }

  const std::array rows{*definition};
  return database_
      .executeBulk(
          "INSERT INTO definition(symbol_id,file_id,offset,size) "
          "VALUES(?1,?2,?3,?4) "
          "ON CONFLICT(symbol_id) DO UPDATE SET file_id=excluded.file_id,"
          "offset=excluded.offset,size=excluded.size",
          rows,
          storage::detail::typedBinder(
              [id, file](auto bind, const Region &region) {
                return bind(id, file, region.offset, region.size);
              }))
      .transform([](const storage::BulkResult &) {});
}

} // namespace facts
