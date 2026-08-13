// Location.h — functional conversion from a Clang location to the fact model.

#ifndef FACTS_TOOL_AST_EXTRACTORS_LOCATION_H
#define FACTS_TOOL_AST_EXTRACTORS_LOCATION_H

#include "ast/extractors/Extraction.h"
#include "model/Location.h"

namespace clang {
class LangOptions;
class SourceLocation;
class SourceManager;
class SourceRange;
} // namespace clang

namespace facts {

// Normalizes and validates sourceLocation before converting it to the model.
ExtractionResult<Location>
extractLocation(const clang::SourceManager &sourceManager,
                clang::SourceLocation sourceLocation);

ExtractionResult<Region>
extractRegion(const clang::SourceManager &sourceManager,
              const clang::LangOptions &langOptions,
              clang::SourceRange sourceRange);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_LOCATION_H
