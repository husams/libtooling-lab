#include "ast/extractors/Location.h"

#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

namespace facts {
namespace {

ExtractionResult<clang::SourceLocation>
normalizeLocation(const clang::SourceManager &sourceManager,
                  clang::SourceLocation sourceLocation) {
  sourceLocation = sourceManager.getExpansionLoc(sourceLocation);
  if (sourceLocation.isInvalid()) {
    return std::unexpected(ExtractionError::InvalidSourceLocation);
  }
  if (sourceManager.isInSystemHeader(sourceLocation)) {
    return std::unexpected(ExtractionError::SystemHeader);
  }

  return sourceLocation;
}

ExtractionResult<Location> toLocation(const clang::SourceManager &sourceManager,
                                      clang::SourceLocation sourceLocation) {

  clang::PresumedLoc presumed = sourceManager.getPresumedLoc(sourceLocation);
  if (presumed.isInvalid()) {
    return std::unexpected(ExtractionError::InvalidPresumedLocation);
  }

  return Location{
      .line = presumed.getLine(),
      .column = presumed.getColumn(),
      .offset = sourceManager.getFileOffset(sourceLocation),
  };
}

struct NormalizedRange {
  clang::SourceLocation begin;
  clang::SourceLocation end;
};

ExtractionResult<Region> toRegion(const clang::SourceManager &sourceManager,
                                  const clang::LangOptions &langOptions,
                                  NormalizedRange sourceRange) {
  const auto end = clang::Lexer::getLocForEndOfToken(
      sourceRange.end, 0, sourceManager, langOptions);
  if (end.isInvalid()) {
    return std::unexpected(ExtractionError::InvalidSourceRange);
  }

  const auto beginOffset = sourceManager.getFileOffset(sourceRange.begin);
  const auto endOffset = sourceManager.getFileOffset(end);
  if (endOffset < beginOffset) {
    return std::unexpected(ExtractionError::InvalidSourceRange);
  }

  return Region{
      .offset = beginOffset,
      .size = endOffset - beginOffset,
  };
}

} // namespace

ExtractionResult<Location>
extractLocation(const clang::SourceManager &sourceManager,
                clang::SourceLocation sourceLocation) {
  const auto toModel = [&](clang::SourceLocation normalizedLocation) {
    return toLocation(sourceManager, normalizedLocation);
  };

  return normalizeLocation(sourceManager, sourceLocation) | toModel;
}

ExtractionResult<Region>
extractRegion(const clang::SourceManager &sourceManager,
              const clang::LangOptions &langOptions,
              clang::SourceRange sourceRange) {
  const auto normalizeEnd = [&](clang::SourceLocation begin) {
    const auto toNormalizedRange =
        [begin](
            clang::SourceLocation end) -> ExtractionResult<NormalizedRange> {
      return NormalizedRange{.begin = begin, .end = end};
    };

    return normalizeLocation(sourceManager, sourceRange.getEnd()) |
           toNormalizedRange;
  };
  const auto toModel = [&](NormalizedRange normalizedRange) {
    return toRegion(sourceManager, langOptions, normalizedRange);
  };

  return normalizeLocation(sourceManager, sourceRange.getBegin()) |
         normalizeEnd | toModel;
}

} // namespace facts
