#include "storage/Symbol.h"

#include "storage/ItlibGenerator.h"
#include "storage/SemanticProperties.h"
#include "storage/StorageQuery.h"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace facts {
namespace {

std::expected<SymbolId, std::error_code>
allocateSymbolId(storage::Database &database, FileId file) {
  auto ids = storage::detail::toItlibGenerator(database.query(
      "INSERT INTO symbol_allocator(file_id,next_index) VALUES(?1,1) "
      "ON CONFLICT(file_id) DO UPDATE SET next_index=next_index+1 "
      "RETURNING next_index-1",
      [](const storage::Row &row) { return row.get<std::int64_t>(0); }, file));
  return storage::detail::collectOne(std::move(ids))
      .and_then(
          [file](std::int64_t raw) -> std::expected<SymbolId, std::error_code> {
            if (raw < 0 || raw > std::numeric_limits<std::uint32_t>::max()) {
              return std::unexpected(
                  std::make_error_code(std::errc::value_too_large));
            }
            return SymbolId{file, static_cast<std::uint32_t>(raw)};
          });
}

} // namespace

std::expected<void, std::error_code>
Storage::replaceSymbolRow(SymbolId id, SymbolNode node, const Symbol &symbol) {
  const std::array rows{symbol};
  return database_
      .executeBulk(
          "INSERT INTO symbol(id,node,kind,sub_kind,"
          "lang,properties,usr,qualified_name,line,col,offset,access,is_"
          "definition,"
          "is_implicit,is_static,is_virtual,is_const,is_inline,is_pure,"
          "ref_qualifier,is_override,has_internal_linkage,is_external,is_"
          "variadic,"
          "is_deleted,is_defaulted,is_explicit,is_final,is_abstract,is_"
          "polymorphic,"
          "has_extern_storage,constant_evaluation,is_noexcept,is_volatile) "
          "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,"
          "?17,?18,?19,?20,?21,?22,?23,?24,?25,?26,?27,?28,?29,?30,?31,?32,"
          "?33,?34) "
          "ON CONFLICT(id) DO UPDATE SET node=excluded.node,"
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
          "has_extern_storage=MAX(has_extern_storage,excluded.has_extern_"
          "storage),"
          "constant_evaluation=CASE WHEN excluded.constant_evaluation<>'none' "
          "THEN excluded.constant_evaluation ELSE constant_evaluation END,"
          "is_noexcept=MAX(is_noexcept,excluded.is_noexcept),"
          "is_volatile=MAX(is_volatile,excluded.is_volatile)",
          rows,
          storage::detail::typedBinder([id, node](auto bind,
                                                  const Symbol &value) {
            const auto properties = storage::symbolProperties(value.flags);
            return bind(id, node, storage::storedSymbolKind(value.Kind),
                        value.SubKind, value.Lang, value.Properties, value.usr,
                        value.qualifiedName, value.loc.line, value.loc.column,
                        value.loc.offset, properties.access,
                        properties.isDefinition, properties.isImplicit,
                        properties.isStatic, properties.isVirtual,
                        properties.isConst, properties.isInline,
                        properties.isPure, properties.refQualifier,
                        properties.isOverride, properties.hasInternalLinkage,
                        properties.isExternal, properties.isVariadic,
                        properties.isDeleted, properties.isDefaulted,
                        properties.isExplicit, properties.isFinal,
                        properties.isAbstract, properties.isPolymorphic,
                        properties.hasExternStorage,
                        properties.constantEvaluation, properties.isNoexcept,
                        properties.isVolatile);
          }))
      .transform([](const storage::BulkResult &) {});
}

std::expected<Symbol, std::error_code> Storage::loadSymbolRow(SymbolNode node,
                                                              SymbolId id) {
  auto rows = storage::detail::toItlibGenerator(database_.query(
      "SELECT kind,sub_kind,lang,properties,usr,qualified_name,line,col,"
      "offset,access,is_definition,is_implicit,is_static,is_virtual,is_const,"
      "is_inline,is_pure,ref_qualifier,is_override,has_internal_linkage,"
      "is_external,is_variadic,is_deleted,is_defaulted,is_explicit,is_final,"
      "is_abstract,is_polymorphic,has_extern_storage,constant_evaluation,"
      "is_noexcept,is_volatile "
      "FROM symbol WHERE id=?1 AND node=?2",
      [id](const storage::Row &row) {
        Symbol symbol;
        symbol.id = id;
        symbol.Kind = storage::symbolKindFromStored(row.get<std::int64_t>(0));
        symbol.SubKind = row.get<decltype(symbol.SubKind)>(1);
        symbol.Lang = row.get<decltype(symbol.Lang)>(2);
        symbol.Properties = row.get<decltype(symbol.Properties)>(3);
        symbol.usr = row.get<std::string>(4);
        symbol.qualifiedName = row.get<std::string>(5);
        symbol.loc = {row.get<unsigned>(6), row.get<unsigned>(7),
                      row.get<unsigned>(8)};
        symbol.flags = storage::symbolFlags({
            .access = row.get<std::string>(9),
            .isDefinition = row.get<bool>(10),
            .isImplicit = row.get<bool>(11),
            .isStatic = row.get<bool>(12),
            .isVirtual = row.get<bool>(13),
            .isConst = row.get<bool>(14),
            .isInline = row.get<bool>(15),
            .isPure = row.get<bool>(16),
            .refQualifier = row.get<std::string>(17),
            .isOverride = row.get<bool>(18),
            .hasInternalLinkage = row.get<bool>(19),
            .isExternal = row.get<bool>(20),
            .isVariadic = row.get<bool>(21),
            .isDeleted = row.get<bool>(22),
            .isDefaulted = row.get<bool>(23),
            .isExplicit = row.get<bool>(24),
            .isFinal = row.get<bool>(25),
            .isAbstract = row.get<bool>(26),
            .isPolymorphic = row.get<bool>(27),
            .hasExternStorage = row.get<bool>(28),
            .constantEvaluation = row.get<std::string>(29),
            .isNoexcept = row.get<bool>(30),
            .isVolatile = row.get<bool>(31),
        });
        return symbol;
      },
      id, node));
  return storage::detail::collectOne(std::move(rows));
}

std::expected<std::optional<SymbolId>, std::error_code>
Storage::findId(std::string_view usr) {
  if (usr.empty()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  auto ids = storage::detail::toItlibGenerator(database_.query(
      "SELECT id FROM symbol WHERE usr=?1",
      [](const storage::Row &row) { return row.get<SymbolId>(0); }, usr));
  return storage::detail::collectOptional(std::move(ids));
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
  if (symbol.usr.empty()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  auto transaction = writeTransaction();
  if (!transaction) {
    return std::unexpected(transaction.error());
  }

  auto id =
      findId(symbol.usr)
          .and_then([&](std::optional<SymbolId> existing)
                        -> std::expected<SymbolId, std::error_code> {
            return existing
                       ? std::expected<SymbolId, std::error_code>{*existing}
                       : allocateSymbolId(database_, symbol.id.file);
          });

  return id
      .and_then([&](SymbolId id) {
        return replaceSymbolRow(id, node, symbol)
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
        return commit(*transaction).transform([id] { return id; });
      });
}

std::expected<Symbol, std::error_code>
Storage::loadSymbol(SymbolNode node, SymbolId id, SymbolFacts facts) {
  auto transaction = readTransaction();
  if (!transaction) {
    return std::unexpected(transaction.error());
  }

  return loadSymbolRow(node, id)
      .and_then([this, facts](Symbol symbol) {
        return loadFacts(std::move(symbol), facts);
      })
      .and_then([&](Symbol symbol) {
        return commit(*transaction)
            .transform([symbol = std::move(symbol)]() mutable {
              return std::move(symbol);
            });
      });
}

std::expected<std::optional<Symbol>, std::error_code>
Storage::loadSymbol(SymbolNode node, std::string_view usr, SymbolFacts facts) {
  if (usr.empty()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  auto transaction = readTransaction();
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
        return commit(*transaction)
            .transform([symbol = std::move(symbol)]() mutable {
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
