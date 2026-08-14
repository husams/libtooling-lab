#include "ast/extractors/FunctionTemplate.h"

#include "ast/extractors/TemplatePattern.h"
#include "ast/extractors/TemplateSpecialization.h"
#include "storage/FactStore.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclTemplate.h>

#include <utility>
#include <vector>

namespace facts {

ExtractionResult<FunctionTemplate>
toFunctionTemplate(Function function,
                   const clang::TemplateParameterList &parameters,
                   FactStore &store) {
  return extractTemplateArguments(parameters, store)
      .transform([function = std::move(function)](
                     std::vector<TemplateArgument> arguments) mutable {
        FunctionTemplate result;
        static_cast<Function &>(result) = std::move(function);
        result.templateArguments = std::move(arguments);
        return result;
      });
}

ExtractionResult<FunctionInstance>
toFunctionInstance(Function function, const clang::FunctionDecl &node,
                   FactStore &store) {
  const auto *arguments = node.getTemplateSpecializationArgs();
  if (arguments == nullptr) {
    return std::unexpected(ExtractionError::InvalidType);
  }

  return extractTemplateParameters(arguments->asArray(), node.getASTContext(),
                                   store)
      .transform([function = std::move(function)](
                     std::vector<TemplateParameter> parameters) mutable {
        FunctionInstance instance;
        static_cast<Function &>(instance) = std::move(function);
        instance.templateParameters = std::move(parameters);
        return instance;
      });
}

std::expected<void, std::error_code>
storeFunctionInstanceRelations(const clang::FunctionDecl &node,
                               SymbolId instance, FactStore &store) {
  const auto *specialization = node.getTemplateSpecializationInfo();
  const auto *arguments = node.getTemplateSpecializationArgs();
  if (specialization == nullptr || arguments == nullptr) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  return storeTemplateInstanceRelations(
      instance, *specialization->getTemplate()->getTemplatedDecl(),
      specialization->getTemplateSpecializationKind(), arguments->asArray(),
      node.getASTContext(), store);
}

} // namespace facts
