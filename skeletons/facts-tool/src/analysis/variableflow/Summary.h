#pragma once

#include "analysis/variableflow/FlowSupport.h"

#include <unordered_map>
#include <vector>

namespace facts::variableflow::detail {

struct Argument {
  std::vector<std::int64_t> sources;
  const clang::VarDecl *variable = nullptr;
  bool address = false;
};

struct Call {
  const clang::CallExpr *expression = nullptr;
  std::int64_t node = 0;
  std::vector<Argument> arguments;
};

// One context-insensitive summary per function: occurrences and entry
// parameters are shared, while argument/return/effect edges retain the
// originating callsite.
struct Summary {
  const Function *function = nullptr;
  unsigned depth = 0;
  Blocks blocks;
  std::int64_t owner = 0;
  std::vector<std::int64_t> nodes;
  std::unordered_map<const clang::VarDecl *, std::vector<std::int64_t>>
      variables;
  std::unordered_map<std::int64_t, const clang::VarDecl *> variableOf;
  std::unordered_map<const clang::ParmVarDecl *, std::int64_t> parameters;
  std::unordered_map<const clang::ParmVarDecl *, std::int64_t> memoryParameters;
  std::unordered_map<const clang::ParmVarDecl *, std::vector<std::int64_t>>
      effects;
  std::vector<std::int64_t> returns;
  std::vector<Call> calls;
};

// Collect every local occurrence once. Traversal activates only the connected
// variables, expressions and calls; unrelated statements are removed at finish.
Summary collectSummary(const Function &, Builder &, unsigned depth);
int blockFor(const clang::Stmt *, const Function &, const Blocks &);
int orderFor(const clang::Stmt *, const Function &, const Blocks &);

} // namespace facts::variableflow::detail
