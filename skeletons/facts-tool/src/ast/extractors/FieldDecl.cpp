#include "ast/extractors/FieldDecl.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/Initializer.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/ValueDecl.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>

#include <utility>

namespace facts {
namespace {

ExtractionResult<Field>
addFieldDetails(Field field, const clang::FieldDecl &node,
                const clang::SourceManager &sourceManager) {
  field.initializer =
      extractInitializer(node.getInClassInitializer(), node.getType(),
                         node.getASTContext(), sourceManager);
  auto toDefinedField =
      [field = std::move(field), access = node.getAccess(),
       isConst = node.getType().isConstQualified()](
          Region definition) mutable -> ExtractionResult<Field> {
    field.definition = definition;
    field.flags = (field.flags & ~accessMask) |
                  static_cast<std::uint32_t>(access) | bit(DefinitionBit) |
                  (isConst ? bit(ConstBit) : 0U);
    return std::move(field);
  };

  return extractRegion(sourceManager, node.getASTContext().getLangOpts(),
                       node.getSourceRange()) |
         std::move(toDefinedField);
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
    return storeValueRelations(node, field, store);
  };

  storeExtracted(node, extractField(node, context.getSourceManager()), context,
                 files, store, storeRelation);
}

} // namespace facts
