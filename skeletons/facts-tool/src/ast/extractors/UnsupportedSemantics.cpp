#include "ast/extractors/UnsupportedSemantics.h"

#include "ast/extractors/File.h"
#include "cli/Verbose.h"
#include "storage/FactStore.h"

#include <clang/Basic/SourceManager.h>

#include <format>

namespace facts {

void reportUnsupportedSemantic(std::string_view kind,
                               clang::SourceLocation location,
                               const clang::SourceManager &sourceManager,
                               FileManager &files, const FactStore &store) {
  const auto expansion = sourceManager.getExpansionLoc(location);
  if (!resolveFile(sourceManager, expansion, files))
    return;
  const auto presumed = sourceManager.getPresumedLoc(expansion);
  const auto site = presumed.isValid()
                        ? std::format("{}:{}:{}", presumed.getFilename(),
                                      presumed.getLine(), presumed.getColumn())
                        : std::string{"unknown"};
  cli::logVerbose(store.verbosity(), 0,
                  "facts-tool: coverage.unsupported_semantics kind={} site={}",
                  kind, site);
}

} // namespace facts
