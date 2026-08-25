#ifndef FACTS_TOOL_PLATFORMFLAGS_H
#define FACTS_TOOL_PLATFORMFLAGS_H

#include <expected>
#include <memory>
#include <span>
#include <string>

namespace clang::tooling {
class CompilationDatabase;
} // namespace clang::tooling

namespace facts {

std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
configurePlatformCompilationDatabase(
    const clang::tooling::CompilationDatabase &database,
    std::span<const std::string> sources);

} // namespace facts

#endif // FACTS_TOOL_PLATFORMFLAGS_H
