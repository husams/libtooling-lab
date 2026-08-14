#include "ast/extractors/FunctionDecl.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/Definition.h"
#include "ast/extractors/FunctionTemplate.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/MethodDecl.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/Parameters.h"
#include "model/AnySymbol.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"

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

  const auto addFlags = [&](Function function) {
    return addMethodFlags(std::move(function), node);
  };

  return extractSymbol<Symbol, clang::NamedDecl>(node, sourceManager)
      .and_then(toFunction)
      .and_then(addFlags)
      .and_then(addDefinition)
      .and_then(addParameterList);
}

void collectSymbol(clang::FunctionDecl &node, clang::ASTContext &context,
                   FileManager &files, FactStore &store) {
  const auto storeRelations = [&](SymbolId function) {
    return storeMethodRelation(node, function, store);
  };

  if (node.getTemplateSpecializationInfo() != nullptr) {
    const auto toInstance = [&](Function function) {
      return toFunctionInstance(std::move(function), node, store);
    };
    const auto storeInstanceRelations = [&](SymbolId function) {
      return storeMethodRelation(node, function, store).and_then([&] {
        return storeFunctionInstanceRelations(node, function, store);
      });
    };

    storeExtracted(node,
                   extractFunction(node, context.getSourceManager(), store) |
                       toInstance,
                   context, files, store, storeInstanceRelations);
    return;
  }

  if (const auto *templateDeclaration = node.getDescribedFunctionTemplate()) {
    const auto toTemplate = [&](Function function) {
      return toFunctionTemplate(std::move(function),
                                *templateDeclaration->getTemplateParameters(),
                                store);
    };

    storeExtracted(node,
                   extractFunction(node, context.getSourceManager(), store) |
                       toTemplate,
                   context, files, store, storeRelations);
    return;
  }

  storeExtracted(node, extractFunction(node, context.getSourceManager(), store),
                 context, files, store, storeRelations);
}

} // namespace facts
