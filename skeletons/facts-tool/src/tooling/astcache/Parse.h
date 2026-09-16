#pragma once

#include <memory>
#include <string>
#include <vector>

namespace clang {
class ASTUnit;
namespace tooling {
class ClangTool;
}
}

namespace facts::astcache::detail {
int parseWithLookups(clang::tooling::ClangTool &tool,
                     std::vector<std::unique_ptr<clang::ASTUnit>> &units,
                     std::vector<std::string> &lookupNames);
} // namespace facts::astcache::detail
