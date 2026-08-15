#include "ast/extractors/EnumDecl.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/Definition.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/Type.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>

#include <utility>

namespace facts {
namespace {

ExtractionResult<SymbolId> extractUnderlyingType(const clang::EnumDecl &node,
                                                 FactStore &store) {
  const auto type = node.getIntegerType();
  if (type.isNull()) {
    return SymbolId{};
  }
  return extractType(type, store).transform_error([](std::error_code) {
    return ExtractionError::InvalidType;
  });
}

ExtractionResult<Enumeration> addEnumerationDetails(Enumeration enumeration,
                                                    const clang::EnumDecl &node,
                                                    FactStore &store) {
  return extractUnderlyingType(node, store)
      .transform([enumeration = std::move(enumeration),
                  &node](SymbolId underlyingType) mutable {
        enumeration.underlyingType = underlyingType;
        enumeration.isScoped = node.isScoped();
        enumeration.hasFixedUnderlyingType = node.isFixed();
        return std::move(enumeration);
      });
}

} // namespace

ExtractionResult<Enumeration>
extractEnumeration(const clang::EnumDecl &node,
                   const clang::SourceManager &sourceManager,
                   FactStore &store) {
  const auto toEnumeration =
      [](Symbol symbol) -> ExtractionResult<Enumeration> {
    return toSymbolModel<Enumeration>(std::move(symbol));
  };
  const auto addDetails = [&](Enumeration enumeration) {
    return addEnumerationDetails(std::move(enumeration), node, store);
  };
  const auto addDefinition = [&](Enumeration enumeration) {
    return addDefinitionRegion(std::move(enumeration), node,
                               node.isCompleteDefinition(), sourceManager);
  };

  return extractSymbol<Symbol, clang::NamedDecl>(node, sourceManager)
      .and_then(toEnumeration)
      .and_then(addDetails)
      .and_then(addDefinition);
}

void collectSymbol(clang::EnumDecl &node, clang::ASTContext &context,
                   FileManager &files, FactStore &store) {
  storeExtracted(node,
                 extractEnumeration(node, context.getSourceManager(), store),
                 context, files, store);
}

} // namespace facts
