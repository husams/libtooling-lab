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

// The part of this machine that decides which headers a compile command sees:
// the resource directory, the SDK root and the Clang the tool links against.
// Import records it; extraction compares against it, so a registry that no
// longer matches its toolchain is reported as such instead of as a random
// missing header.
std::string platformFingerprint();

} // namespace facts

#endif // FACTS_TOOL_PLATFORMFLAGS_H
