#ifndef FACTS_TOOL_AST_EXTRACTORS_RECORDINSTANCE_H
#define FACTS_TOOL_AST_EXTRACTORS_RECORDINSTANCE_H

#include "ast/extractors/Extraction.h"
#include "model/RecordInstance.h"

#include <expected>
#include <system_error>

namespace clang {
class ClassTemplateSpecializationDecl;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;

ExtractionResult<RecordInstance>
extractRecordInstance(const clang::ClassTemplateSpecializationDecl &node,
                      const clang::SourceManager &sourceManager,
                      FactStore &store);

std::expected<void, std::error_code>
storeRecordInstanceRelations(const clang::ClassTemplateSpecializationDecl &node,
                             SymbolId instance, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_RECORDINSTANCE_H
