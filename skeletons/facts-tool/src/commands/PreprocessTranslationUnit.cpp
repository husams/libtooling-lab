#include "commands/PreprocessTranslationUnit.h"

#include "tooling/astcache/Cache.h"

#include <clang/Tooling/Tooling.h>

namespace facts::commands {
std::expected<IncludeGraphFacts, std::string> preprocessTranslationUnit(
    const clang::tooling::CompilationDatabase &database,
    const std::string &source, const astcache::Options &cache) {
  if (auto cached = astcache::cachedIncludes(database, source, cache))
    return std::move(*cached);
  // Each source owns a fresh FileManager so relative filenames from distinct
  // compilation directories cannot share stale metadata.
  IncludeGraphFacts includes;
  clang::tooling::ClangTool tool(database, {source});
  if (tool.run(createIncludeVisitorFactory(includes).get()) != 0)
    return std::unexpected("cannot enumerate included files: " +
                           clang::tooling::getAbsolutePath(source) +
                           " failed to preprocess; fix the compile commands "
                           "and import again");
  return includes;
}
}
