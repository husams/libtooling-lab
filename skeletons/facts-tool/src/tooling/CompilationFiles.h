#ifndef FACTS_TOOL_TOOLING_COMPILATION_FILES_H
#define FACTS_TOOL_TOOLING_COMPILATION_FILES_H

#include <expected>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace clang::tooling {
class CompilationDatabase;
}

namespace facts {

std::expected<std::vector<std::string>, std::error_code>
discoverCompilationFiles(
    const clang::tooling::CompilationDatabase &compilations,
    std::span<const std::string> fallbackSources);

} // namespace facts

#endif // FACTS_TOOL_TOOLING_COMPILATION_FILES_H
