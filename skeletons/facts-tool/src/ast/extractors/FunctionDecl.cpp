#include "ast/extractors/FunctionDecl.h"

#include "model/AnySymbol.h"

#include "ast/extractors/Definition.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/Parameters.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"

#include <utility>
#include <vector>

namespace facts {
namespace {

ExtractionResult<Function>
addParameters(Function function, const clang::FunctionDecl &node,
              const clang::SourceManager &sourceManager, FactStore &store) {
  auto toFunction = [function = std::move(function)](
                        std::vector<Parameter> parameters) mutable
      -> ExtractionResult<Function> {
    function.parameters = std::move(parameters);
    return std::move(function);
  };

  return extractParameters(node, sourceManager, store) | std::move(toFunction);
}

} // namespace

ExtractionResult<Function>
extractFunction(const clang::FunctionDecl &node,
                const clang::SourceManager &sourceManager, FactStore &store) {
  const auto toFunction = [](Symbol symbol) -> ExtractionResult<Function> {
    return toSymbolModel<Function>(std::move(symbol));
  };
  const auto addDefinition = [&](Function function) {
    return addDefinitionRegion(std::move(function), node,
                               node.isThisDeclarationADefinition(),
                               sourceManager);
  };
  const auto addParameterList = [&](Function function) {
    return addParameters(std::move(function), node, sourceManager, store);
  };

  return extractSymbol<Symbol, clang::NamedDecl>(node, sourceManager) |
         toFunction | addDefinition | addParameterList;
}

} // namespace facts
