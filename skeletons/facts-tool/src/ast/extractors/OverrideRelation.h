#pragma once

#include "analysis/callgraph/CallGraphTypes.h"
#include "ast/extractors/Extraction.h"

#include <vector>

namespace clang {
class CXXMethodDecl;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

ExtractionResult<std::vector<callgraph::OverrideFact>>
extractOverrideRelations(const clang::CXXMethodDecl &method,
                         const clang::SourceManager &sourceManager,
                         FileManager &files, FactStore &store);

} // namespace facts
