#pragma once

#include <clang/Basic/SourceLocation.h>

#include <string_view>

namespace clang {
class SourceManager;
}

namespace facts {

void reportUnsupportedSemantic(std::string_view kind,
                               clang::SourceLocation location,
                               const clang::SourceManager &sourceManager);

} // namespace facts
