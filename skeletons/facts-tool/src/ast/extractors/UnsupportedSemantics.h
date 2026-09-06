#pragma once

#include <clang/Basic/SourceLocation.h>

#include <string_view>

namespace clang {
class SourceManager;
}

namespace facts {
class FactStore;
class FileManager;

void reportUnsupportedSemantic(std::string_view kind,
                               clang::SourceLocation location,
                               const clang::SourceManager &sourceManager,
                               FileManager &files, const FactStore &store);

} // namespace facts
