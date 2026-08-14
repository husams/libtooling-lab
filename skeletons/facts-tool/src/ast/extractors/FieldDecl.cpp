#include "ast/extractors/FieldDecl.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"
#include "model/AnySymbol.h"
#include "model/Relation.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>

#include <array>
#include <system_error>
#include <utility>

namespace facts {
namespace {

ExtractionResult<Field>
addFieldDetails(Field field, const clang::FieldDecl &node,
                const clang::SourceManager &sourceManager) {
  auto toDefinedField =
      [field = std::move(field), access = node.getAccess()](
          Region definition) mutable -> ExtractionResult<Field> {
    field.definition = definition;
    field.flags = (field.flags & ~accessMask) |
                  static_cast<std::uint32_t>(access) | bit(DefinitionBit);
    return std::move(field);
  };

  return extractRegion(sourceManager, node.getASTContext().getLangOpts(),
                       node.getSourceRange()) |
         std::move(toDefinedField);
}

std::expected<void, std::error_code>
storeFieldRelation(const clang::FieldDecl &node, SymbolId field,
                   FactStore &store) {
  const auto invalidUsr = [](ExtractionError) {
    return std::make_error_code(std::errc::invalid_argument);
  };
  const auto findOwner = [&store](std::string ownerUsr) {
    return store.findId(ownerUsr);
  };
  const auto storeRelation = [field, &store](std::optional<SymbolId> owner)
      -> std::expected<void, std::error_code> {
    if (!owner) {
      return std::unexpected(
          std::make_error_code(std::errc::no_such_file_or_directory));
    }

    const std::array relations{Relation{
        .source = field,
        .destination = *owner,
        .kind = RelationKind::FieldOf,
    }};
    return store.addRelations(relations);
  };

  return extractUsr(*node.getParent())
      .transform_error(invalidUsr)
      .and_then(findOwner)
      .and_then(storeRelation);
}

} // namespace

ExtractionResult<Field>
extractField(const clang::FieldDecl &node,
             const clang::SourceManager &sourceManager) {
  const auto toField = [](Symbol symbol) -> ExtractionResult<Field> {
    return toSymbolModel<Field>(std::move(symbol));
  };
  const auto addDetails = [&](Field field) {
    return addFieldDetails(std::move(field), node, sourceManager);
  };

  return extractSymbol<Symbol, clang::NamedDecl>(node, sourceManager)
      .and_then(toField)
      .and_then(addDetails);
}

void collectSymbol(clang::FieldDecl &node, clang::ASTContext &context,
                   FileManager &files, FactStore &store) {
  const auto storeRelation = [&](SymbolId field) {
    return storeFieldRelation(node, field, store);
  };

  storeExtracted(node, extractField(node, context.getSourceManager()), context,
                 files, store, storeRelation);
}

} // namespace facts
