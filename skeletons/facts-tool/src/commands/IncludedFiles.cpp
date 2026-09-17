#include "commands/IncludedFiles.h"

#include "commands/PreprocessTranslationUnit.h"
#include "platform/PlatformFlags.h"
#include "tooling/astcache/Cache.h"

#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace facts::commands {
namespace {

// ClangTool only announces progress when it processes more than one file, so
// the per-source tools stay silent and the span keeps the same lines it had.
void announceProgress(std::size_t index, std::size_t total,
                      const std::string &source) {
  if (total > 1) {
    llvm::errs() << "[" << index + 1 << "/" << total << "] Processing file "
                 << clang::tooling::getAbsolutePath(source) << ".\n";
  }
}

std::expected<IncludeGraphFacts, std::string> discoverSourceIncludes(
    const clang::tooling::CompilationDatabase &database,
    const std::string &source, const astcache::Options &cache,
    IncludeDiscovery discovery) {
  if (!cache.enabled || discovery == IncludeDiscovery::Dependencies)
    return preprocessTranslationUnit(database, source, cache);
  IncludeGraphFacts includes;
  if (astcache::prepareAST(database, source, includes, cache) != 0)
    return std::unexpected("cannot prepare imported source: " +
                           clang::tooling::getAbsolutePath(source) +
                           " failed to preprocess; fix the compile commands "
                           "and import again");
  return includes;
}

} // namespace

std::expected<DiscoveredIncludes, std::string> discoverIncludedFilesPerSource(
    const clang::tooling::CompilationDatabase &compilations,
    std::span<const std::string> selectedSources,
    const astcache::Options &cache, IncludeDiscovery discovery) {
  return configurePlatformCompilationDatabase(compilations, selectedSources)
      .transform_error([](std::string error) {
        return "cannot resolve included files: " + std::move(error);
      })
      .and_then([&](auto configured)
                    -> std::expected<DiscoveredIncludes, std::string> {
        DiscoveredIncludes result;
        IncludeGraphFacts merged;
        for (std::size_t index = 0; index < selectedSources.size(); ++index) {
          const auto &source = selectedSources[index];
          announceProgress(index, selectedSources.size(), source);
          auto preprocessed =
              discoverSourceIncludes(*configured, source, cache, discovery);
          if (!preprocessed)
            return std::unexpected(std::move(preprocessed.error()));
          auto facts = std::move(*preprocessed);
          auto owned = facts.visitedSources;
          std::ranges::sort(owned);
          owned.erase(std::ranges::unique(owned).begin(), owned.end());
          result.perSource.emplace(source, std::move(owned));
          std::ranges::move(facts.visitedSources,
                            std::back_inserter(merged.visitedSources));
          std::ranges::move(facts.edges, std::back_inserter(merged.edges));
        }
        std::ranges::sort(merged.visitedSources);
        merged.visitedSources.erase(
            std::ranges::unique(merged.visitedSources).begin(),
            merged.visitedSources.end());
        result.merged = std::move(merged.visitedSources);
        return result;
      });
}

std::expected<std::vector<std::string>, std::string>
discoverIncludedFiles(const clang::tooling::CompilationDatabase &compilations,
                      std::span<const std::string> selectedSources,
                      const astcache::Options &cache, IncludeDiscovery discovery) {
  return discoverIncludedFilesPerSource(compilations, selectedSources, cache,
                                       discovery)
      .transform([](DiscoveredIncludes discovered) {
        return std::move(discovered.merged);
      });
}

} // namespace facts::commands
