#include "commands/IncludedFiles.h"

#include "ast/visitors/IncludeVisitor.h"
#include "platform/PlatformFlags.h"

#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include <algorithm>
#include <ranges>
#include <utility>

namespace facts::commands {

std::expected<std::vector<std::string>, std::string>
discoverIncludedFiles(const clang::tooling::CompilationDatabase &compilations,
                      std::span<const std::string> selectedSources) {
  return configurePlatformCompilationDatabase(compilations, selectedSources)
      .transform_error([](std::string error) {
        return "cannot resolve included files: " + std::move(error);
      })
      .and_then([&](auto configured)
                    -> std::expected<std::vector<std::string>, std::string> {
        IncludeGraphFacts facts;
        clang::tooling::ClangTool tool(*configured, selectedSources);
        if (tool.run(createIncludeVisitorFactory(facts).get()) != 0) {
          return std::unexpected(
              "cannot enumerate included files: at least one translation unit "
              "failed to preprocess; fix the compile commands and import "
              "again");
        }
        std::ranges::sort(facts.visitedSources);
        facts.visitedSources.erase(
            std::ranges::unique(facts.visitedSources).begin(),
            facts.visitedSources.end());
        return std::move(facts.visitedSources);
      });
}

} // namespace facts::commands
