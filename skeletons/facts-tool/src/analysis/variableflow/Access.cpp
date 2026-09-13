#include "analysis/variableflow/Collector.h"

#include <clang/AST/ASTContext.h>

namespace facts::variableflow::detail {
namespace {

template <typename Lookup>
int enclosing(const clang::Stmt *statement, const Function &function,
              const Lookup &lookup) {
  std::unordered_set<const clang::Stmt *> seen;
  while (statement != nullptr && seen.insert(statement).second) {
    if (const auto found = lookup.find(statement); found != lookup.end())
      return static_cast<int>(found->second);
    const auto parents = function.context->getParents(*statement);
    statement = parents.empty() ? nullptr : parents[0].get<clang::Stmt>();
  }
  return -1;
}

} // namespace

int blockFor(const clang::Stmt *stmt, const Function &function,
             const Blocks &blocks) {
  return enclosing(stmt, function, blocks.statements);
}

int orderFor(const clang::Stmt *stmt, const Function &function,
             const Blocks &blocks) {
  return enclosing(stmt, function, blocks.statementOrder);
}

Collector::Collector(const Function &function, Builder &builder, unsigned depth)
    : function_(function), builder_(builder) {
  summary_.function = &function;
  summary_.depth = depth;
  summary_.blocks = buildBlocks(function, builder, depth);
  for (const auto &[ignored, id] : summary_.blocks.nodes)
    summary_.nodes.push_back(id);
  summary_.owner = node("function", nullptr, function.decl->getLocation(), -1);
}

std::int64_t Collector::node(std::string kind, const clang::VarDecl *variable,
                             clang::SourceLocation location, int block,
                             const std::string &name, bool memory) {
  const auto id = builder_.node(std::move(kind), function_, variable, location,
                                block, summary_.depth, name, memory);
  summary_.nodes.push_back(id);
  return id;
}

void Collector::remember(std::int64_t id, const clang::VarDecl *variable) {
  summary_.variables[variable].push_back(id);
  summary_.variableOf[id] = variable;
}

std::int64_t Collector::access(Storage storage, const clang::Stmt *anchor,
                               clang::SourceLocation location, Access kind) {
  const auto block = blockFor(anchor, function_, summary_.blocks);
  const auto id = node(kind == Access::Read    ? "read"
                       : kind == Access::Write ? "write"
                                               : "update",
                       storage.variable, location, block, {}, storage.memory);
  builder_.event(id, function_, storage.variable, kind, block,
                 orderFor(anchor, function_, summary_.blocks), storage.memory);
  remember(id, storage.variable);
  const auto *parameter = llvm::dyn_cast<clang::ParmVarDecl>(storage.variable);
  if (kind != Access::Read && parameter != nullptr &&
      (parameter->getType()->isReferenceType() || storage.memory))
    summary_.effects[parameter].push_back(id);
  return id;
}

const clang::VarDecl *
Collector::canonical(const clang::VarDecl *variable) const {
  std::unordered_set<const clang::VarDecl *> seen;
  while (variable != nullptr && seen.insert(variable).second) {
    const auto found = references_.find(variable);
    if (found == references_.end())
      break;
    variable = found->second;
  }
  return variable;
}

Storage Collector::pointed(const clang::VarDecl *variable) const {
  variable = canonical(variable);
  const auto found = pointers_.find(variable);
  return found == pointers_.end() ? Storage{variable, true} : found->second;
}

Storage Collector::storage(const clang::Expr *expr) const {
  expr = strip(expr);
  if (const auto *ref = llvm::dyn_cast_or_null<clang::DeclRefExpr>(expr))
    return {canonical(llvm::dyn_cast<clang::VarDecl>(ref->getDecl())), false};
  if (const auto *unary = llvm::dyn_cast_or_null<clang::UnaryOperator>(expr)) {
    if (unary->getOpcode() == clang::UO_AddrOf)
      return storage(unary->getSubExpr());
    if (unary->getOpcode() == clang::UO_Deref) {
      const auto base = storage(unary->getSubExpr());
      return base.variable == nullptr || base.memory ? Storage{}
                                                     : pointed(base.variable);
    }
  }
  return {};
}

void Collector::pointerBinding(const clang::VarDecl *variable,
                               const clang::Expr *expr) {
  variable = canonical(variable);
  pointers_.erase(variable);
  expr = strip(expr);
  if (const auto *address = llvm::dyn_cast_or_null<clang::UnaryOperator>(expr);
      address != nullptr && address->getOpcode() == clang::UO_AddrOf) {
    const auto target = storage(address->getSubExpr());
    if (target.variable != nullptr)
      pointers_[variable] = target;
    return;
  }
  const auto source = storage(expr);
  if (source.variable != nullptr && source.variable->getType()->isPointerType())
    pointers_[variable] = pointed(source.variable);
}

void Collector::dependencies(const std::vector<std::int64_t> &sources,
                             std::int64_t target) {
  for (const auto id : sources) {
    const auto &source =
        builder_.graph.nodes.at(static_cast<std::size_t>(id - 1));
    const bool call = source.kind == "call";
    builder_.edge(id, target, call ? "call-result" : "value", call ? id : 0);
  }
}

} // namespace facts::variableflow::detail
