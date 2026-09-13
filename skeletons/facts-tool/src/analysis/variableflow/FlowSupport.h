#pragma once

#include "analysis/variableflow/Internal.h"

#include <clang/AST/ASTTypeTraits.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ParentMapContext.h>
#include <cstddef>
#include <deque>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace facts::variableflow::detail {

enum class Access { Read, Write, Update, MayWrite };

struct Blocks {
  std::unordered_map<const clang::Stmt *, int> statements;
  std::unordered_map<const clang::Stmt *, unsigned> statementOrder;
  std::unordered_map<int, std::int64_t> nodes;
  std::unordered_map<int, std::vector<int>> predecessors;
  std::unordered_set<int> reachable;
  int entry = -1;
};

struct AccessEvent {
  std::int64_t node = 0;
  std::string functionUsr;
  std::string variableUsr;
  Access kind;
  int block = -1;
  int statementOrder = -1;
  std::size_t sequence = 0;
  bool memory = false;
};

struct NodeKey {
  std::string functionUsr;
  std::string kind;
  std::string variableUsr;
  std::string file;
  std::uint64_t offset = 0;
  int block = -1;
  std::string name;
  bool memory = false;

  bool operator==(const NodeKey &) const = default;
};

struct NodeKeyHash {
  std::size_t operator()(const NodeKey &key) const noexcept;
};

struct EdgeKey {
  std::int64_t source = 0;
  std::int64_t target = 0;
  std::string kind;
  std::int64_t callsite = 0;

  bool operator==(const EdgeKey &) const = default;
};

struct EdgeKeyHash {
  std::size_t operator()(const EdgeKey &key) const noexcept;
};

struct BoundaryKey {
  std::int64_t node = 0;
  std::string reason;
  std::string detail;
  unsigned depth = 0;

  bool operator==(const BoundaryKey &) const = default;
};

struct BoundaryKeyHash {
  std::size_t operator()(const BoundaryKey &key) const noexcept;
};

struct Builder {
  Graph graph;
  std::int64_t nextId = 1;
  std::vector<AccessEvent> events;
  std::unordered_map<std::string, Blocks> blockGraphs;
  std::size_t nextEvent = 0;
  std::unordered_map<NodeKey, std::int64_t, NodeKeyHash> nodeIds;
  std::unordered_set<EdgeKey, EdgeKeyHash> edges;
  std::unordered_set<BoundaryKey, BoundaryKeyHash> boundaries;
  std::unordered_set<std::int64_t> eventNodes;

  std::int64_t node(std::string kind, const Function &function,
                    const clang::VarDecl *variable, clang::SourceLocation loc,
                    int block, unsigned depth, const std::string &name = {},
                    bool memory = false);

  void edge(std::int64_t source, std::int64_t target, std::string kind,
            std::int64_t callsite = 0);

  void boundary(std::int64_t nodeId, std::string reason, std::string detail,
                unsigned depth);

  void event(std::int64_t id, const Function &function,
             const clang::VarDecl *variable, Access kind, int block,
             int statementOrder = -1, bool memory = false);
};

Blocks buildBlocks(const Function &function, Builder &builder, unsigned depth);

inline const clang::Expr *strip(const clang::Expr *expression) {
  return expression ? expression->IgnoreParenImpCasts() : nullptr;
}

} // namespace facts::variableflow::detail
