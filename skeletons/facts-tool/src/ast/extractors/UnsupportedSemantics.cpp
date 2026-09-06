#include "ast/extractors/UnsupportedSemantics.h"

#include <clang/Basic/SourceManager.h>
#include <llvm/Support/raw_ostream.h>

namespace facts {

void reportUnsupportedSemantic(std::string_view kind,
                               clang::SourceLocation location,
                               const clang::SourceManager &sourceManager) {
  const auto presumed =
      sourceManager.getPresumedLoc(sourceManager.getExpansionLoc(location));
  llvm::errs() << "facts-tool: coverage.unsupported_semantics kind=" << kind
               << " site=";
  if (presumed.isValid())
    llvm::errs() << presumed.getFilename() << ':' << presumed.getLine() << ':'
                 << presumed.getColumn();
  else
    llvm::errs() << "unknown";
  llvm::errs() << '\n';
}

} // namespace facts
