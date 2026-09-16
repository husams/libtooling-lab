#pragma once

#include "ast/visitors/IncludeVisitor.h"

namespace clang {
class ASTUnit;
}

namespace facts::astcache {

// Detailed preprocessing records are serialized along with the AST, preserving
// include edges even when headers are skipped by include guards.
IncludeGraphFacts includesFromAST(clang::ASTUnit &unit);

} // namespace facts::astcache
