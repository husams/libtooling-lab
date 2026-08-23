#include "storage/Enumeration.h"

#include "storage/ItlibGenerator.h"
#include "storage/StorageQuery.h"

#include <array>

namespace facts {

std::expected<void, std::error_code>
Storage::replaceEnumerationDetails(SymbolId id,
                                   const Enumeration &enumeration) {
  const std::array rows{enumeration};
  return database_
      .executeBulk(
          "INSERT INTO enumeration(symbol_id,underlying_type,is_scoped,"
          "has_fixed_underlying_type) VALUES(?1,?2,?3,?4) "
          "ON CONFLICT(symbol_id) DO UPDATE SET "
          "underlying_type=excluded.underlying_type,"
          "is_scoped=excluded.is_scoped,"
          "has_fixed_underlying_type=excluded.has_fixed_underlying_type",
          rows,
          storage::detail::typedBinder(
              [id](auto bind, const Enumeration &value) {
                return bind(id, value.underlyingType, value.isScoped,
                            value.hasFixedUnderlyingType);
              }))
      .transform([](const storage::BulkResult &) {});
}

std::expected<Enumeration, std::error_code>
Storage::loadEnumerationDetails(Enumeration enumeration) {
  const auto id = enumeration.id;
  auto details = storage::detail::toItlibGenerator(database_.query(
      "SELECT underlying_type,is_scoped,has_fixed_underlying_type "
      "FROM enumeration WHERE symbol_id=?1",
      [enumeration = std::move(enumeration)](const storage::Row &row) mutable {
        enumeration.underlyingType = row.get<SymbolId>(0);
        enumeration.isScoped = row.get<bool>(1);
        enumeration.hasFixedUnderlyingType = row.get<bool>(2);
        return std::move(enumeration);
      },
      id));
  return storage::detail::collectOne(std::move(details));
}

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Enumeration>(const Enumeration &enumeration) {
  return saveModel(SymbolNode::Enumeration, enumeration, {.definition = true})
      .and_then([this, &enumeration](SymbolId id) {
        return replaceEnumerationDetails(id, enumeration).transform([id] {
          return id;
        });
      });
}

template <>
std::expected<Enumeration, std::error_code>
Storage::load<Enumeration>(SymbolId id) {
  return loadModel<Enumeration>(SymbolNode::Enumeration, id,
                                {.definition = true})
      .and_then([this](Enumeration enumeration) {
        return loadEnumerationDetails(std::move(enumeration));
      });
}

template <>
std::expected<std::optional<Enumeration>, std::error_code>
Storage::load<Enumeration>(std::string_view usr) {
  return loadModel<Enumeration>(SymbolNode::Enumeration, usr,
                                {.definition = true})
      .and_then(
          [this](std::optional<Enumeration> enumeration)
              -> std::expected<std::optional<Enumeration>, std::error_code> {
            if (!enumeration) {
              return std::nullopt;
            }
            return loadEnumerationDetails(std::move(*enumeration))
                .transform([](Enumeration value) {
                  return std::optional<Enumeration>{std::move(value)};
                });
          });
}

} // namespace facts
