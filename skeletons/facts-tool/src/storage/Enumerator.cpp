#include "storage/Enumerator.h"

#include "storage/Sqlite.h"

#include <sqlite3.h>

namespace facts {

std::expected<void, std::error_code>
Storage::replaceEnumeratorDetails(SymbolId id, const Enumerator &enumerator) {
  auto statement = storage::prepare(
      handle(),
      "INSERT INTO enumerator(symbol_id,value,initializer_expression) "
      "VALUES(?1,?2,?3) ON CONFLICT(symbol_id) DO UPDATE SET "
      "value=excluded.value,"
      "initializer_expression=excluded.initializer_expression");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id)) ||
      !storage::bindText(statement->get(), 2, enumerator.value) ||
      !storage::bindText(statement->get(), 3,
                         enumerator.initializerExpression.value_or("")) ||
      sqlite3_step(statement->get()) != SQLITE_DONE) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  return {};
}

std::expected<Enumerator, std::error_code>
Storage::loadEnumeratorDetails(Enumerator enumerator) {
  auto statement = storage::prepare(
      handle(), "SELECT value,initializer_expression FROM enumerator "
                "WHERE symbol_id=?1");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1,
                            storage::packSymbolId(enumerator.id))) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  const auto step = sqlite3_step(statement->get());
  if (step == SQLITE_DONE) {
    return std::unexpected(
        std::make_error_code(std::errc::no_such_file_or_directory));
  }
  if (step != SQLITE_ROW) {
    return std::unexpected(storage::sqliteError(handle()));
  }

  enumerator.value = storage::columnText(statement->get(), 0);
  auto expression = storage::columnText(statement->get(), 1);
  enumerator.initializerExpression =
      expression.empty() ? std::nullopt
                         : std::optional<std::string>{std::move(expression)};
  return enumerator;
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
