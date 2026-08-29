#ifndef FACTS_TOOL_AST_EXTRACTORS_TEMPLATESPECIALIZATION_H
#define FACTS_TOOL_AST_EXTRACTORS_TEMPLATESPECIALIZATION_H

#include "ast/Indexing.h"
#include "ast/extractors/Extraction.h"
#include "ast/extractors/Type.h"
#include "model/SymbolId.h"
#include "model/TemplateParameter.h"

#include <clang/Basic/Specifiers.h>
#include <llvm/ADT/ArrayRef.h>

#include <expected>
#include <string_view>
#include <system_error>
#include <vector>

namespace clang {
class ASTContext;
class NamedDecl;
class TemplateArgument;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

DetailedExtractionResult<std::vector<TemplateParameter>>
extractTemplateParameters(llvm::ArrayRef<clang::TemplateArgument> arguments,
                          const clang::ASTContext &context, FileManager &files,
                          FactStore &store);

IndexingResult storeTemplateInstanceRelations(
    SymbolId instance, std::string_view instanceName,
    const clang::NamedDecl &pattern,
    clang::TemplateSpecializationKind specializationKind,
    llvm::ArrayRef<clang::TemplateArgument> arguments,
    const clang::ASTContext &context, FileManager &files, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_TEMPLATESPECIALIZATION_H
