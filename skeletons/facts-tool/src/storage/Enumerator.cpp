#include "storage/Enumerator.h"

#include "storage/ItlibGenerator.h"
#include "storage/StorageQuery.h"

#include <array>

namespace facts {

std::expected<void, std::error_code>
Storage::replaceEnumeratorDetails(SymbolId id, const Enumerator &enumerator) {
  const std::array rows{enumerator};
  return database_
      .executeBulk(
          "INSERT INTO enumerator(symbol_id,value,initializer_expression) "
          "VALUES(?1,?2,?3) ON CONFLICT(symbol_id) DO UPDATE SET "
          "value=excluded.value,"
          "initializer_expression=excluded.initializer_expression",
          rows,
          [id](sqlite3_stmt *statement, const Enumerator &value) {
            return storage::bindParameters(
                statement, id, value.value,
                value.initializerExpression.value_or(""));
          })
      .transform([](const storage::BulkResult &) {});
}

std::expected<Enumerator, std::error_code>
Storage::loadEnumeratorDetails(Enumerator enumerator) {
  const auto id = enumerator.id;
  auto details = storage::detail::toItlibGenerator(database_.query(
      "SELECT value,initializer_expression FROM enumerator "
      "WHERE symbol_id=?1",
      [enumerator = std::move(enumerator)](const storage::Row &row) mutable {
        enumerator.value = row.get<std::string>(0);
        auto expression = row.get<std::string>(1);
        enumerator.initializerExpression =
            expression.empty()
                ? std::nullopt
                : std::optional<std::string>{std::move(expression)};
        return std::move(enumerator);
      },
      id));
  return storage::detail::collectOne(std::move(details));
}

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Enumerator>(const Enumerator &enumerator) {
  return saveModel(SymbolNode::Enumerator, enumerator, {.definition = true})
      .and_then([this, &enumerator](SymbolId id) {
        return replaceEnumeratorDetails(id, enumerator).transform([id] {
          return id;
        });
      });
}

template <>
std::expected<Enumerator, std::error_code>
Storage::load<Enumerator>(SymbolId id) {
  return loadModel<Enumerator>(SymbolNode::Enumerator, id, {.definition = true})
      .and_then([this](Enumerator enumerator) {
        return loadEnumeratorDetails(std::move(enumerator));
      });
}

template <>
std::expected<std::optional<Enumerator>, std::error_code>
Storage::load<Enumerator>(std::string_view usr) {
  return loadModel<Enumerator>(SymbolNode::Enumerator, usr,
                               {.definition = true})
      .and_then(
          [this](std::optional<Enumerator> enumerator)
              -> std::expected<std::optional<Enumerator>, std::error_code> {
            if (!enumerator) {
              return std::nullopt;
            }
            return loadEnumeratorDetails(std::move(*enumerator))
                .transform([](Enumerator value) {
                  return std::optional<Enumerator>{std::move(value)};
                });
          });
}

} // namespace facts
