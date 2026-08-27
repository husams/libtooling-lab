#ifndef FACTS_TOOL_COMMANDS_INCLUDED_FILES_H
#define FACTS_TOOL_COMMANDS_INCLUDED_FILES_H

#include <expected>
#include <span>
#include <string>
#include <vector>

namespace clang::tooling {
class CompilationDatabase;
}

namespace facts::commands {

std::expected<std::vector<std::string>, std::string>
discoverIncludedFiles(const clang::tooling::CompilationDatabase &compilations,
                      std::span<const std::string> selectedSources);

} // namespace facts::commands

#endif // FACTS_TOOL_COMMANDS_INCLUDED_FILES_H
