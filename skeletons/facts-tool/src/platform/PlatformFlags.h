// platform/PlatformFlags.h — make the embedded front-end able to find headers.
//
// The embedded front-end can't locate headers on its own; point it at what's
// already installed. Every ClangTool this process creates goes through here.

#ifndef FACTS_TOOL_PLATFORMFLAGS_H
#define FACTS_TOOL_PLATFORMFLAGS_H

namespace clang::tooling {
class ClangTool;
} // namespace clang::tooling

namespace facts {

void addPlatformFlags(clang::tooling::ClangTool &tool);

} // namespace facts

#endif // FACTS_TOOL_PLATFORMFLAGS_H
