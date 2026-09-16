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
struct RevisionObservations;
int parsePersistent(clang::tooling::ClangTool &tool,
                    std::vector<std::unique_ptr<clang::ASTUnit>> &units,
                    RevisionObservations *observations = nullptr);
} // namespace facts::astcache::detail
