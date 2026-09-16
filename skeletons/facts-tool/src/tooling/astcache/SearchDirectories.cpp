#include "tooling/astcache/SearchDirectories.h"

#include "tooling/astcache/FileIdentity.h"

#include <clang/Lex/HeaderSearch.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/Preprocessor.h>

#include <algorithm>

namespace facts::astcache::detail {
std::vector<fs::path>
headerSearchDirectories(clang::Preprocessor &preprocessor,
                        const fs::path &workingDirectory) {
  std::vector<fs::path> directories;
  const auto &search = preprocessor.getHeaderSearchInfo();
  for (const auto &lookup : search.search_dir_range()) {
    const auto path = resolve(lookup.getName().str(), workingDirectory);
    directories.push_back(lookup.isHeaderMap() ? path.parent_path() : path);
  }
  // Preserve repositories containing currently missing include directories.
  for (const auto &lookup : search.getHeaderSearchOpts().UserEntries)
    directories.push_back(resolve(lookup.Path, workingDirectory));
  std::ranges::sort(directories);
  directories.erase(std::ranges::unique(directories).begin(), directories.end());
  return directories;
}
} // namespace facts::astcache::detail
