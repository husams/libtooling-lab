#include "storage/Enumeration.h"

#include "storage/Sqlite.h"

#include <sqlite3.h>

namespace facts {

std::expected<void, std::error_code>
Storage::replaceEnumerationDetails(SymbolId id,
                                   const Enumeration &enumeration) {
  auto statement = storage::prepare(
      handle(), "INSERT INTO enumeration(symbol_id,underlying_type,is_scoped,"
                "has_fixed_underlying_type) VALUES(?1,?2,?3,?4) "
                "ON CONFLICT(symbol_id) DO UPDATE SET "
                "underlying_type=excluded.underlying_type,"
                "is_scoped=excluded.is_scoped,"
                "has_fixed_underlying_type=excluded.has_fixed_underlying_type");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id)) ||
      !storage::bindInteger(
          statement->get(), 2,
          storage::packSymbolId(enumeration.underlyingType)) ||
      !storage::bindInteger(statement->get(), 3, enumeration.isScoped) ||
      !storage::bindInteger(statement->get(), 4,
                            enumeration.hasFixedUnderlyingType) ||
      sqlite3_step(statement->get()) != SQLITE_DONE) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  return {};
}

std::expected<Enumeration, std::error_code>
Storage::loadEnumerationDetails(Enumeration enumeration) {
  auto statement = storage::prepare(
      handle(), "SELECT underlying_type,is_scoped,has_fixed_underlying_type "
                "FROM enumeration WHERE symbol_id=?1");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1,
                            storage::packSymbolId(enumeration.id))) {
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

  enumeration.underlyingType =
      storage::unpackSymbolId(sqlite3_column_int64(statement->get(), 0));
  enumeration.isScoped = sqlite3_column_int64(statement->get(), 1) != 0;
  enumeration.hasFixedUnderlyingType =
      sqlite3_column_int64(statement->get(), 2) != 0;
  return enumeration;
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
