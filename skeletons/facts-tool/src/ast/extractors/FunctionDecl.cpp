#include "ast/extractors/FunctionDecl.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/Definition.h"
#include "ast/extractors/CallableProperties.h"
#include "ast/extractors/FunctionTemplate.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/MethodDecl.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/Parameters.h"
#include "ast/extractors/ReturnType.h"
#include "model/AnySymbol.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"

#include <utility>
#include <vector>

namespace facts {
namespace {

DetailedExtractionResult<Function>
addParameters(Function function, const clang::FunctionDecl &node,
              const clang::SourceManager &sourceManager, FileManager &files,
              FactStore &store) {
  auto toFunction = [function = std::move(function)](
                        std::vector<Parameter> parameters) mutable
      -> DetailedExtractionResult<Function> {
    function.parameters = std::move(parameters);
    return std::move(function);
  };

  return extractParameters(node, sourceManager, files, store) |
         std::move(toFunction);
}

} // namespace

DetailedExtractionResult<Function>
extractFunction(const clang::FunctionDecl &node,
                const clang::SourceManager &sourceManager, FileManager &files,
                FactStore &store) {
  const auto toFunction = [](Symbol symbol) -> ExtractionResult<Function> {
    return toSymbolModel<Function>(std::move(symbol));
  };
  const auto addDefinition = [&](Function function) {
    return addDefinitionRegion(std::move(function), node,
                               node.isThisDeclarationADefinition(),
                               sourceManager);
  };
  const auto addParameterList = [&](Function function) {
    return addParameters(std::move(function), node, sourceManager, files,
                         store);
  };

  const auto addFlags = [&](Function function) {
    return addCallableProperties(std::move(function), node)
        .and_then([&](Function value) {
          return addMethodFlags(std::move(value), node);
        });
  };

  return extractSymbol<Symbol, clang::NamedDecl>(node, sourceManager)
      .and_then(toFunction)
      .and_then(addFlags)
      .and_then(addDefinition)
      .transform_error(
          [](ExtractionError error) { return DetailedExtractionError{error}; })
      .and_then(addParameterList);
}

IndexingResult collectSymbol(clang::FunctionDecl &node,
                             clang::ASTContext &context, FileManager &files,
                             FactStore &store) {
  const auto storeRelations = [&](SymbolId function) {
    return storeMethodRelation(node, function, context.getSourceManager(),
                               files, store)
        .and_then([&] { return storeReturnType(node, function, files, store); });
  };

  if (node.getTemplateSpecializationInfo() != nullptr) {
    const auto toInstance = [&](Function function) {
      return toFunctionInstance(std::move(function), node, files, store);
    };
    const auto storeInstanceRelations = [&](SymbolId function) {
      return storeRelations(function)
          .and_then([&] {
            return storeFunctionInstanceRelations(node, function, files, store);
          });
    };

    return storeExtracted(
        node,
        extractFunction(node, context.getSourceManager(), files, store) |
            toInstance,
        context, files, store, storeInstanceRelations);
  }

  if (const auto *templateDeclaration = node.getDescribedFunctionTemplate()) {
    const auto toTemplate = [&](Function function) {
      return toFunctionTemplate(std::move(function),
                                *templateDeclaration->getTemplateParameters(),
                                context.getSourceManager(), files, store);
    };

    return storeExtracted(
        node,
        extractFunction(node, context.getSourceManager(), files, store) |
            toTemplate,
        context, files, store, storeRelations);
  }

  return storeExtracted(
      node, extractFunction(node, context.getSourceManager(), files, store),
      context, files, store, storeRelations);
}

} // namespace facts
