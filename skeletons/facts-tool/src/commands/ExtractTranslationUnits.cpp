#include "commands/ExtractTranslationUnits.h"

#include "ast/FactExtractor.h"
#include "ast/visitors/Traversal.h"
#include "tooling/FrontendActivity.h"
#include "tooling/astcache/Cache.h"

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>

namespace facts::commands {
int extractTranslationUnits(const clang::tooling::CompilationDatabase &database,
                            const std::vector<std::string> &sources,
                            FileManager &files, FactStore &store,
                            IndexingStatus &status,
                            const astcache::Options &cache) {
  if (!cache.enabled) {
    for (const auto &source : sources)
      reportFrontendActivity(cache.verbosity, "ast-parse", source);
    clang::tooling::ClangTool tool(database, sources);
    return tool.run(createFactExtractorFactory(files, store, status).get());
  }
  // Consume one TU at a time: caching must not retain the entire project's
  // ASTs in memory during extraction.
  for (const auto &source : sources) {
    std::vector<std::unique_ptr<clang::ASTUnit>> units;
    const auto result = astcache::buildASTs(database, {source}, units, cache);
    if (result != 0 || units.empty())
      return result != 0 ? result : 1;
    for (const auto &unit : units)
      traverse(unit->getASTContext(), files, store, status);
  }
  return 0;
}
}
