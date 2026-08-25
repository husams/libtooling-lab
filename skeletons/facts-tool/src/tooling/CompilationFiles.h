#ifndef FACTS_TOOL_TOOLING_COMPILATION_FILES_H
#define FACTS_TOOL_TOOLING_COMPILATION_FILES_H

#include <expected>
#include <span>
#include <string>
#include <vector>

namespace clang::tooling {
class CompilationDatabase;
}

namespace facts {

struct CompilationFiles {
  std::vector<std::string> files;
  std::vector<std::string> diagnostics;
};

std::expected<CompilationFiles, std::string> discoverCompilationFiles(
    const clang::tooling::CompilationDatabase &compilations,
    std::span<const std::string> selectedSources);

} // namespace facts

#endif // FACTS_TOOL_TOOLING_COMPILATION_FILES_H
