#ifndef FACTS_TOOL_AST_EXTRACTORS_RECORDDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_RECORDDECL_H

#include "ast/extractors/Extraction.h"
#include "model/Record.h"

namespace clang {
class CXXRecordDecl;
class SourceManager;
} // namespace clang

namespace facts {

ExtractionResult<Record>
extractRecord(const clang::CXXRecordDecl &node,
              const clang::SourceManager &sourceManager);

} // namespace facts

#endif
