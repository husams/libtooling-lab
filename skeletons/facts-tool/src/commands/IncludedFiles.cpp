#include "commands/IncludedFiles.h"

#include "ast/visitors/IncludeVisitor.h"
#include "platform/PlatformFlags.h"

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

// Every selected source gets a ClangTool of its own. ClangTool::run shares one
// FileManager across all of its invocations and only switches the working
// directory between them, so two compile commands spelling the same relative
// path under different directories reuse the first command's FileEntry: the
// second file is then read with the first file's size, which zero-fills the
// buffer on a read and faults on an mmap. A fresh tool owns a fresh
// FileManager, so neither a source nor a header spelling can inherit another
// working directory's metadata.
std::expected<void, std::string>
preprocessOne(const clang::tooling::CompilationDatabase &configured,
              const std::string &source, IncludeGraphFacts &facts) {
  clang::tooling::ClangTool tool(configured, std::vector<std::string>{source});
  if (tool.run(createIncludeVisitorFactory(facts).get()) != 0) {
    return std::unexpected("cannot enumerate included files: " +
                           clang::tooling::getAbsolutePath(source) +
                           " failed to preprocess; fix the compile commands "
                           "and import again");
  }
  return {};
}

// ClangTool only announces progress when it processes more than one file, so
// the per-source tools stay silent and the span keeps the same lines it had.
void announceProgress(std::size_t index, std::size_t total,
                      const std::string &source) {
  if (total > 1) {
    llvm::errs() << "[" << index + 1 << "/" << total << "] Processing file "
                 << clang::tooling::getAbsolutePath(source) << ".\n";
  }
}

} // namespace

std::expected<DiscoveredIncludes, std::string> discoverIncludedFilesPerSource(
    const clang::tooling::CompilationDatabase &compilations,
    std::span<const std::string> selectedSources) {
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
          IncludeGraphFacts facts;
          if (auto preprocessed = preprocessOne(*configured, source, facts);
              !preprocessed) {
            return std::unexpected(std::move(preprocessed.error()));
          }
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
                      std::span<const std::string> selectedSources) {
  return discoverIncludedFilesPerSource(compilations, selectedSources)
      .transform([](DiscoveredIncludes discovered) {
        return std::move(discovered.merged);
      });
}

} // namespace facts::commands
