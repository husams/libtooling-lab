#include "ast/extractors/FunctionDecl.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/NamedDecl.h"
#include "model/FunctionTemplate.h"
#include "model/Relation.h"

#include <array>
#include <system_error>

#include <clang/AST/DeclCXX.h>

#include "model/AnySymbol.h"

#include "ast/extractors/Definition.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/Parameters.h"
#include "ast/extractors/Type.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"

#include <ranges>
#include <utility>
#include <vector>

namespace facts {
namespace {

std::uint32_t flagWhen(SymbolBit flag, bool condition) {
  return condition ? bit(flag) : 0;
}

std::uint32_t flagWhen(TemplateArgumentBit flag, bool condition) {
  return condition ? bit(static_cast<std::size_t>(flag)) : 0;
}

ExtractionResult<TemplateArgument>
extractTemplateArgument(const clang::NamedDecl &node, FactStore &store) {
  if (const auto *parameter =
          llvm::dyn_cast<clang::TemplateTypeParmDecl>(&node)) {
    return TemplateArgument{
        .name = parameter->getNameAsString(),
        .flags = flagWhen(ParameterPackBit, parameter->isParameterPack()),
    };
  }

  if (const auto *parameter =
          llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(&node)) {
    return extractType(parameter->getType(), store)
        .transform_error(
            [](std::error_code) { return ExtractionError::InvalidType; })
        .transform([&](SymbolId type) {
          return TemplateArgument{
              .name = parameter->getNameAsString(),
              .type = type,
              .flags =
                  flagWhen(ParameterPackBit, parameter->isParameterPack()) |
                  bit(static_cast<std::size_t>(NonTypeBit)),
          };
        });
  }

  if (const auto *parameter =
          llvm::dyn_cast<clang::TemplateTemplateParmDecl>(&node)) {
    return TemplateArgument{
        .name = parameter->getNameAsString(),
        .flags = flagWhen(ParameterPackBit, parameter->isParameterPack()) |
                 bit(static_cast<std::size_t>(TemplateTemplateBit)),
    };
  }

  return std::unexpected(ExtractionError::InvalidType);
}

const clang::CXXMethodDecl *supportedMethod(const clang::FunctionDecl &node) {
  const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(&node);
  if (!method || llvm::isa<clang::CXXConstructorDecl, clang::CXXDestructorDecl,
                           clang::CXXConversionDecl>(method)) {
    return nullptr;
  }
  return method;
}

ExtractionResult<Function> addMethodFlags(Function function,
                                          const clang::FunctionDecl &node) {
  const auto *method = supportedMethod(node);
  if (!method) {
    return function;
  }

  function.flags |=
      static_cast<std::uint32_t>(method->getAccess()) |
      flagWhen(VirtualBit, method->isVirtual()) |
      flagWhen(PureBit, method->isPureVirtual()) |
      flagWhen(OverrideBit, method->size_overridden_methods() != 0) |
      flagWhen(InlineBit, method->isInlined()) |
      flagWhen(DefaultedBit, method->isDefaulted()) |
      flagWhen(DeletedBit, method->isDeleted());
  return function;
}

std::expected<void, std::error_code>
storeMethodRelation(const clang::FunctionDecl &node, SymbolId function,
                    FactStore &store) {
  const auto *method = supportedMethod(node);
  if (!method) {
    return {};
  }

  const auto invalidUsr = [](ExtractionError) {
    return std::make_error_code(std::errc::invalid_argument);
  };
  const auto findOwner = [&store](std::string ownerUsr) {
    return store.findId(ownerUsr);
  };
  const auto storeRelation = [function, &store](std::optional<SymbolId> owner)
      -> std::expected<void, std::error_code> {
    if (!owner) {
      return std::unexpected(
          std::make_error_code(std::errc::no_such_file_or_directory));
    }

    const std::array relations{Relation{
        .source = function,
        .destination = *owner,
        .kind = RelationKind::MethodOf,
    }};
    return store.addRelations(relations);
  };

  return extractUsr(*method->getParent())
      .transform_error(invalidUsr)
      .and_then(findOwner)
      .and_then(storeRelation);
}

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

ExtractionResult<std::vector<TemplateArgument>>
extractTemplateArguments(const clang::TemplateParameterList &parameters,
                         FactStore &store) {
  const auto append =
      [&](ExtractionResult<std::vector<TemplateArgument>> result,
          const clang::NamedDecl *parameter) {
        return std::move(result).and_then(
            [&](std::vector<TemplateArgument> arguments) {
              return extractTemplateArgument(*parameter, store)
                  .transform([arguments = std::move(arguments)](
                                 TemplateArgument argument) mutable {
                    arguments.push_back(std::move(argument));
                    return std::move(arguments);
                  });
            });
      };

  return std::ranges::fold_left(parameters,
                                ExtractionResult<std::vector<TemplateArgument>>{
                                    std::vector<TemplateArgument>{}},
                                append);
}

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

  if (const auto *templateDeclaration = node.getDescribedFunctionTemplate()) {
    const auto toTemplate = [&](Function function) {
      return extractTemplateArguments(
                 *templateDeclaration->getTemplateParameters(), store)
          .transform([function = std::move(function)](
                         std::vector<TemplateArgument> arguments) mutable {
            FunctionTemplate result;
            static_cast<Function &>(result) = std::move(function);
            result.templateArguments = std::move(arguments);
            return result;
          });
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
