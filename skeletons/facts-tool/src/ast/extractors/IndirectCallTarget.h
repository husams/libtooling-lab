#ifndef FACTS_TOOL_AST_EXTRACTORS_INDIRECT_CALL_TARGET_H
#define FACTS_TOOL_AST_EXTRACTORS_INDIRECT_CALL_TARGET_H

#include <unordered_set>
#include <vector>

namespace clang {
class CallExpr;
class DeclRefExpr;
class FunctionDecl;
class Stmt;
class VarDecl;
} // namespace clang

namespace facts {

// Collected during the existing body traversal. A non-value-read use can
// mutate or expose a pointer, so it invalidates that local for the whole body.
struct IndirectCallContext {
  std::vector<const clang::Stmt *> ancestors;
  std::unordered_set<const clang::VarDecl *> unsafeVariables;
};

void observeIndirectReference(const clang::DeclRefExpr &reference,
                              IndirectCallContext &context);

const clang::FunctionDecl *
extractIndirectCallTarget(const clang::CallExpr &call,
                          const clang::FunctionDecl &owner,
                          const IndirectCallContext &context);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_INDIRECT_CALL_TARGET_H
