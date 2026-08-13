#include "ast/extractors/FunctionDecl.h"

#include "model/AnySymbol.h"

#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/Parameters.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"

#include <utility>
#include <vector>

namespace facts {
namespace {

ExtractionResult<Symbol>
addDefinitionRegion(Symbol symbol, const clang::FunctionDecl &node,
                    const clang::SourceManager &sourceManager) {
  if (!node.isThisDeclarationADefinition()) {
    return symbol;
  }

  auto toDefinedSymbol =
      [symbol = std::move(symbol)](
          Region definition) mutable -> ExtractionResult<Symbol> {
    symbol.definition = definition;
    symbol.flags |= bit(DefinitionBit);
    return std::move(symbol);
  };

  return extractRegion(sourceManager, node.getASTContext().getLangOpts(),
                       node.getSourceRange()) |
         std::move(toDefinedSymbol);
}

ExtractionResult<Symbol>
addParameters(Symbol symbol, const clang::FunctionDecl &node,
              const clang::SourceManager &sourceManager, FactStore &store) {
  auto toSymbol =
      [symbol = std::move(symbol)](std::vector<Parameter> parameters) mutable
      -> ExtractionResult<Symbol> {
    symbol.parameters = std::move(parameters);
    return std::move(symbol);
  };

  return extractParameters(node, sourceManager, store) | std::move(toSymbol);
}

} // namespace

ExtractionResult<Function>
extractFunction(const clang::FunctionDecl &node,
                const clang::SourceManager &sourceManager, FactStore &store) {
  const auto addDefinition = [&](Symbol symbol) {
    return addDefinitionRegion(std::move(symbol), node, sourceManager);
  };
  const auto addParameterList = [&](Symbol symbol) {
    return addParameters(std::move(symbol), node, sourceManager, store);
  };

  return (extractSymbol<Symbol, clang::NamedDecl>(node, sourceManager) |
          addDefinition | addParameterList)
      .transform([](Symbol symbol) {
        return toSymbolModel<Function>(std::move(symbol));
      });
}

} // namespace facts
