#ifndef FACTS_TOOL_AST_EXTRACTORS_TEMPLATESPECIALIZATION_H
#define FACTS_TOOL_AST_EXTRACTORS_TEMPLATESPECIALIZATION_H

#include "ast/extractors/Extraction.h"
#include "model/SymbolId.h"
#include "model/TemplateParameter.h"

#include <clang/Basic/Specifiers.h>
#include <llvm/ADT/ArrayRef.h>

#include <expected>
#include <system_error>
#include <vector>

namespace clang {
class ASTContext;
class NamedDecl;
class TemplateArgument;
} // namespace clang

namespace facts {
class FactStore;

ExtractionResult<std::vector<TemplateParameter>>
extractTemplateParameters(llvm::ArrayRef<clang::TemplateArgument> arguments,
                          const clang::ASTContext &context, FactStore &store);

std::expected<void, std::error_code> storeTemplateInstanceRelations(
    SymbolId instance, const clang::NamedDecl &pattern,
    clang::TemplateSpecializationKind specializationKind,
    llvm::ArrayRef<clang::TemplateArgument> arguments,
    const clang::ASTContext &context, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_TEMPLATESPECIALIZATION_H
