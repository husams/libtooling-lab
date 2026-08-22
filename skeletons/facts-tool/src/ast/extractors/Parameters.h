#ifndef FACTS_TOOL_AST_EXTRACTORS_PARAMETERS_H
#define FACTS_TOOL_AST_EXTRACTORS_PARAMETERS_H

#include "ast/extractors/Extraction.h"
#include "model/Parameter.h"

#include <vector>

namespace clang {
class FunctionDecl;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

// Extracts parameters from FunctionDecl or any of its subclasses, including
// CXXMethodDecl, while preserving source order.
ExtractionResult<std::vector<Parameter>>
extractParameters(const clang::FunctionDecl &node,
                  const clang::SourceManager &sourceManager, FileManager &files,
                  FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_PARAMETERS_H
