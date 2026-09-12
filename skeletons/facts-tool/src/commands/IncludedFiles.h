#ifndef FACTS_TOOL_COMMANDS_INCLUDED_FILES_H
#define FACTS_TOOL_COMMANDS_INCLUDED_FILES_H

#include <expected>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace clang::tooling {
class CompilationDatabase;
}

namespace facts::commands {

// One preprocessing pass over every selected source, kept in both shapes
// callers need it in: the merged, deduplicated union (registration
// validation) and each source's own transitive include set (the freshness
// check and index-state marking), so nothing gets preprocessed twice.
struct DiscoveredIncludes {
  std::vector<std::string> merged;
  // Keyed by the exact spelling in `selectedSources`; each entry includes
  // that source itself, sorted and deduplicated.
  std::unordered_map<std::string, std::vector<std::string>> perSource;
};

std::expected<DiscoveredIncludes, std::string> discoverIncludedFilesPerSource(
    const clang::tooling::CompilationDatabase &compilations,
    std::span<const std::string> selectedSources);

std::expected<std::vector<std::string>, std::string>
discoverIncludedFiles(const clang::tooling::CompilationDatabase &compilations,
                      std::span<const std::string> selectedSources);

} // namespace facts::commands

#endif // FACTS_TOOL_COMMANDS_INCLUDED_FILES_H
