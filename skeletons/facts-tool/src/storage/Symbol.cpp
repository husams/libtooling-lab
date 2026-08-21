#include "storage/Symbol.h"

#include "storage/SemanticProperties.h"
#include "storage/Sqlite.h"
#include "storage/SymbolIdentity.h"

#include <sqlite3.h>

#include <cstdint>
#include <optional>
#include <utility>

namespace facts {

std::expected<void, std::error_code>
Storage::replaceSymbolRow(SymbolId id, SymbolNode node,
                          std::string_view identity, const Symbol &symbol) {
  const auto properties = storage::symbolProperties(symbol.flags);
  auto statement = storage::prepare(
      handle(),
      "INSERT INTO symbol(id,file_id,file_index,identity,node,kind,sub_kind,"
      "lang,properties,usr,qualified_name,line,col,offset,access,is_definition,"
      "is_implicit,is_static,is_virtual,is_const,is_inline,is_pure,"
      "ref_qualifier,is_override,has_internal_linkage,is_external,is_variadic,"
      "is_deleted,is_defaulted,is_explicit,is_final,is_abstract,is_polymorphic,"
      "has_extern_storage,constant_evaluation,is_noexcept) "
      "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,"
      "?17,?18,?19,?20,?21,?22,?23,?24,?25,?26,?27,?28,?29,?30,?31,?32,"
      "?33,?34,?35,?36) "
      "ON CONFLICT(file_id,identity) DO UPDATE SET node=excluded.node,"
      "kind=excluded.kind,sub_kind=excluded.sub_kind,lang=excluded.lang,"
      "properties=excluded.properties,usr=excluded.usr,"
      "qualified_name=excluded.qualified_name,"
      "access=CASE WHEN excluded.access<>'none' THEN excluded.access ELSE "
      "access END,"
      "is_definition=MAX(is_definition,excluded.is_definition),"
      "is_implicit=MAX(is_implicit,excluded.is_implicit),"
      "is_static=MAX(is_static,excluded.is_static),"
      "is_virtual=MAX(is_virtual,excluded.is_virtual),"
      "is_const=MAX(is_const,excluded.is_const),"
      "is_inline=MAX(is_inline,excluded.is_inline),"
      "is_pure=MAX(is_pure,excluded.is_pure),"
      "ref_qualifier=CASE WHEN excluded.ref_qualifier<>'none' "
      "THEN excluded.ref_qualifier ELSE ref_qualifier END,"
      "is_override=MAX(is_override,excluded.is_override),"
      "has_internal_linkage=MAX(has_internal_linkage,excluded.has_internal_"
      "linkage),"
      "is_external=MIN(is_external,excluded.is_external),"
      "is_variadic=MAX(is_variadic,excluded.is_variadic),"
      "is_deleted=MAX(is_deleted,excluded.is_deleted),"
      "is_defaulted=MAX(is_defaulted,excluded.is_defaulted),"
      "is_explicit=MAX(is_explicit,excluded.is_explicit),"
      "is_final=MAX(is_final,excluded.is_final),"
      "is_abstract=MAX(is_abstract,excluded.is_abstract),"
      "is_polymorphic=MAX(is_polymorphic,excluded.is_polymorphic),"
      "has_extern_storage=MAX(has_extern_storage,excluded.has_extern_storage),"
      "constant_evaluation=CASE WHEN excluded.constant_evaluation<>'none' "
      "THEN excluded.constant_evaluation ELSE constant_evaluation END,"
      "is_noexcept=MAX(is_noexcept,excluded.is_noexcept)");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id)) ||
      !storage::bindInteger(statement->get(), 2, id.file) ||
      !storage::bindInteger(statement->get(), 3, id.index) ||
      !storage::bindText(statement->get(), 4, identity) ||
      !storage::bindInteger(statement->get(), 5, node) ||
      !storage::bindInteger(statement->get(), 6,
                            storage::storedSymbolKind(symbol.Kind)) ||
      !storage::bindInteger(statement->get(), 7, symbol.SubKind) ||
      !storage::bindInteger(statement->get(), 8, symbol.Lang) ||
      !storage::bindInteger(statement->get(), 9, symbol.Properties) ||
      !storage::bindText(statement->get(), 10, symbol.usr) ||
      !storage::bindText(statement->get(), 11, symbol.qualifiedName) ||
      !storage::bindInteger(statement->get(), 12, symbol.loc.line) ||
      !storage::bindInteger(statement->get(), 13, symbol.loc.column) ||
      !storage::bindInteger(statement->get(), 14, symbol.loc.offset) ||
      !storage::bindText(statement->get(), 15, properties.access) ||
      !storage::bindInteger(statement->get(), 16, properties.isDefinition) ||
      !storage::bindInteger(statement->get(), 17, properties.isImplicit) ||
      !storage::bindInteger(statement->get(), 18, properties.isStatic) ||
      !storage::bindInteger(statement->get(), 19, properties.isVirtual) ||
      !storage::bindInteger(statement->get(), 20, properties.isConst) ||
      !storage::bindInteger(statement->get(), 21, properties.isInline) ||
      !storage::bindInteger(statement->get(), 22, properties.isPure) ||
      !storage::bindText(statement->get(), 23, properties.refQualifier) ||
      !storage::bindInteger(statement->get(), 24, properties.isOverride) ||
      !storage::bindInteger(statement->get(), 25,
                            properties.hasInternalLinkage) ||
      !storage::bindInteger(statement->get(), 26, properties.isExternal) ||
      !storage::bindInteger(statement->get(), 27, properties.isVariadic) ||
      !storage::bindInteger(statement->get(), 28, properties.isDeleted) ||
      !storage::bindInteger(statement->get(), 29, properties.isDefaulted) ||
      !storage::bindInteger(statement->get(), 30, properties.isExplicit) ||
      !storage::bindInteger(statement->get(), 31, properties.isFinal) ||
      !storage::bindInteger(statement->get(), 32, properties.isAbstract) ||
      !storage::bindInteger(statement->get(), 33, properties.isPolymorphic) ||
      !storage::bindInteger(statement->get(), 34,
                            properties.hasExternStorage) ||
      !storage::bindText(statement->get(), 35, properties.constantEvaluation) ||
      !storage::bindInteger(statement->get(), 36, properties.isNoexcept) ||
      sqlite3_step(statement->get()) != SQLITE_DONE) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  return {};
}

std::expected<Symbol, std::error_code> Storage::loadSymbolRow(SymbolNode node,
                                                              SymbolId id) {
  auto statement = storage::prepare(
      handle(),
      "SELECT kind,sub_kind,lang,properties,usr,qualified_name,line,col,"
      "offset,access,is_definition,is_implicit,is_static,is_virtual,is_const,"
      "is_inline,is_pure,ref_qualifier,is_override,has_internal_linkage,"
      "is_external,is_variadic,is_deleted,is_defaulted,is_explicit,is_final,"
      "is_abstract,is_polymorphic,has_extern_storage,constant_evaluation,"
      "is_noexcept "
      "FROM symbol WHERE id=?1 AND node=?2");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id)) ||
      !storage::bindInteger(statement->get(), 2, node)) {
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

  Symbol symbol;
  symbol.id = id;
  symbol.Kind =
      storage::symbolKindFromStored(sqlite3_column_int64(statement->get(), 0));
  symbol.SubKind = static_cast<decltype(symbol.SubKind)>(
      sqlite3_column_int64(statement->get(), 1));
  symbol.Lang = static_cast<decltype(symbol.Lang)>(
      sqlite3_column_int64(statement->get(), 2));
  symbol.Properties = static_cast<decltype(symbol.Properties)>(
      sqlite3_column_int64(statement->get(), 3));
  symbol.usr = storage::columnText(statement->get(), 4);
  symbol.qualifiedName = storage::columnText(statement->get(), 5);
  symbol.loc = {
      static_cast<unsigned>(sqlite3_column_int64(statement->get(), 6)),
      static_cast<unsigned>(sqlite3_column_int64(statement->get(), 7)),
      static_cast<unsigned>(sqlite3_column_int64(statement->get(), 8)),
  };
  symbol.flags = storage::symbolFlags({
      .access = storage::columnText(statement->get(), 9),
      .isDefinition = sqlite3_column_int64(statement->get(), 10) != 0,
      .isImplicit = sqlite3_column_int64(statement->get(), 11) != 0,
      .isStatic = sqlite3_column_int64(statement->get(), 12) != 0,
      .isVirtual = sqlite3_column_int64(statement->get(), 13) != 0,
      .isConst = sqlite3_column_int64(statement->get(), 14) != 0,
      .isInline = sqlite3_column_int64(statement->get(), 15) != 0,
      .isPure = sqlite3_column_int64(statement->get(), 16) != 0,
      .refQualifier = storage::columnText(statement->get(), 17),
      .isOverride = sqlite3_column_int64(statement->get(), 18) != 0,
      .hasInternalLinkage = sqlite3_column_int64(statement->get(), 19) != 0,
      .isExternal = sqlite3_column_int64(statement->get(), 20) != 0,
      .isVariadic = sqlite3_column_int64(statement->get(), 21) != 0,
      .isDeleted = sqlite3_column_int64(statement->get(), 22) != 0,
      .isDefaulted = sqlite3_column_int64(statement->get(), 23) != 0,
      .isExplicit = sqlite3_column_int64(statement->get(), 24) != 0,
      .isFinal = sqlite3_column_int64(statement->get(), 25) != 0,
      .isAbstract = sqlite3_column_int64(statement->get(), 26) != 0,
      .isPolymorphic = sqlite3_column_int64(statement->get(), 27) != 0,
      .hasExternStorage = sqlite3_column_int64(statement->get(), 28) != 0,
      .constantEvaluation = storage::columnText(statement->get(), 29),
      .isNoexcept = sqlite3_column_int64(statement->get(), 30) != 0,
  });
  return symbol;
}

std::expected<std::optional<SymbolId>, std::error_code>
Storage::findId(std::string_view usr) {
  if (usr.empty()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  auto statement =
      storage::prepare(handle(), "SELECT file_id,file_index FROM symbol "
                                 "WHERE usr=?1");
  if (!statement || !storage::bindText(statement->get(), 1, usr)) {
    return std::unexpected(storage::sqliteError(handle()));
  }

  const auto step = sqlite3_step(statement->get());
  if (step == SQLITE_DONE) {
    return std::nullopt;
  }
  if (step != SQLITE_ROW) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  return SymbolId{
      static_cast<FileId>(sqlite3_column_int64(statement->get(), 0)),
      static_cast<std::uint32_t>(sqlite3_column_int64(statement->get(), 1)),
  };
}

std::expected<Symbol, std::error_code> Storage::loadFacts(Symbol symbol,
                                                          SymbolFacts facts) {
  const auto id = symbol.id;
  auto definition =
      facts.definition
          ? loadDefinition(id)
          : std::expected<std::optional<DefinitionFacts>, std::error_code>{
                std::nullopt};
  return std::move(definition)
      .transform([symbol = std::move(symbol)](auto loaded) mutable {
        if (loaded) {
          symbol.definition = loaded->region;
          symbol.definitionFile = loaded->file;
        }
        return symbol;
      })
      .and_then([this, id, facts](Symbol value) {
        auto parameters =
            facts.parameters
                ? loadParameters(id)
                : std::expected<std::vector<Parameter>, std::error_code>{};
        return std::move(parameters)
            .transform([value = std::move(value)](auto loaded) mutable {
              value.parameters = std::move(loaded);
              return value;
            });
      });
}

std::expected<SymbolId, std::error_code>
Storage::saveSymbol(SymbolNode node, const Symbol &symbol, SymbolFacts facts) {
  const auto identity = symbolIdentity(symbol);
  auto transaction = storage::Transaction::write(handle());
  if (!transaction) {
    return std::unexpected(transaction.error());
  }

  auto id =
      symbol.usr.empty()
          ? findOrAllocateSymbol(handle(), symbol.id.file, identity)
          : findId(symbol.usr).and_then([&](std::optional<SymbolId> existing) {
              if (existing) {
                return std::expected<SymbolId, std::error_code>{*existing};
              }
              return findOrAllocateSymbol(handle(), symbol.id.file, identity);
            });

  return id
      .and_then([&](SymbolId id) {
        return replaceSymbolRow(id, node, identity, symbol)
            .and_then([&] {
              return replaceDefinition(
                  id, symbol.definitionFile.value_or(symbol.id.file),
                  facts.definition ? symbol.definition : std::nullopt);
            })
            .and_then([&] {
              return replaceParameters(
                  id, facts.parameters
                          ? std::span<const Parameter>{symbol.parameters}
                          : std::span<const Parameter>{});
            })
            .transform([id] { return id; });
      })
      .and_then([&](SymbolId id) {
        return transaction->commit().transform([id] { return id; });
      });
}

std::expected<Symbol, std::error_code>
Storage::loadSymbol(SymbolNode node, SymbolId id, SymbolFacts facts) {
  auto transaction = storage::Transaction::read(handle());
  if (!transaction) {
    return std::unexpected(transaction.error());
  }

  return loadSymbolRow(node, id)
      .and_then([this, facts](Symbol symbol) {
        return loadFacts(std::move(symbol), facts);
      })
      .and_then([&](Symbol symbol) {
        return transaction->commit().transform(
            [symbol = std::move(symbol)]() mutable {
              return std::move(symbol);
            });
      });
}

std::expected<std::optional<Symbol>, std::error_code>
Storage::loadSymbol(SymbolNode node, std::string_view usr, SymbolFacts facts) {
  if (usr.empty()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  auto transaction = storage::Transaction::read(handle());
  if (!transaction) {
    return std::unexpected(transaction.error());
  }

  return findId(usr)
      .and_then([this, node, facts](std::optional<SymbolId> id)
                    -> std::expected<std::optional<Symbol>, std::error_code> {
        if (!id) {
          return std::nullopt;
        }
        return loadSymbolRow(node, *id)
            .and_then([this, facts](Symbol symbol) {
              return loadFacts(std::move(symbol), facts);
            })
            .transform([](Symbol symbol) {
              return std::optional<Symbol>{std::move(symbol)};
            });
      })
      .and_then([&](std::optional<Symbol> symbol) {
        return transaction->commit().transform(
            [symbol = std::move(symbol)]() mutable {
              return std::move(symbol);
            });
      });
}

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Symbol>(const Symbol &symbol) {
  return saveModel(SymbolNode::Symbol, symbol, {});
}

template <>
std::expected<Symbol, std::error_code> Storage::load<Symbol>(SymbolId id) {
  return loadModel<Symbol>(SymbolNode::Symbol, id, {});
}

template <>
std::expected<std::optional<Symbol>, std::error_code>
Storage::load<Symbol>(std::string_view usr) {
  return loadModel<Symbol>(SymbolNode::Symbol, usr, {});
}

} // namespace facts
