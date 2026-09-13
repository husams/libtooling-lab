#include "analysis/variableflow/Collector.h"

namespace facts::variableflow::detail {

void Collector::declaration(const clang::VarDecl *variable,
                            const clang::DeclStmt *stmt) {
  if (!variable->hasInit()) {
    const auto id = node("declaration", variable, variable->getLocation(),
                         blockFor(stmt, function_, summary_.blocks));
    remember(id, variable);
    return;
  }
  const auto values = expression(variable->getInit());
  if (variable->getType()->isReferenceType()) {
    const auto target = storage(variable->getInit());
    const auto id = node("alias", variable, variable->getLocation(),
                         blockFor(stmt, function_, summary_.blocks));
    remember(id, variable);
    for (const auto source : values)
      builder_.edge(source, id, "alias");
    if (target.variable != nullptr && !target.memory)
      references_[variable] = target.variable;
    else
      builder_.boundary(id, "unknown-alias", "reference target is not resolved",
                        summary_.depth);
    return;
  }
  if (variable->getType()->isPointerType())
    pointerBinding(variable, variable->getInit());
  const auto id =
      access({variable, false}, stmt, variable->getLocation(), Access::Write);
  dependencies(values, id);
}

void Collector::statement(const clang::Stmt *stmt) {
  if (stmt == nullptr)
    return;
  if (const auto *expr = llvm::dyn_cast<clang::Expr>(stmt)) {
    expression(expr);
    return;
  }
  if (const auto *decls = llvm::dyn_cast<clang::DeclStmt>(stmt)) {
    for (const auto *decl : decls->decls())
      if (const auto *variable = llvm::dyn_cast<clang::VarDecl>(decl))
        declaration(variable, decls);
    return;
  }
  if (const auto *returned = llvm::dyn_cast<clang::ReturnStmt>(stmt)) {
    const auto values = expression(returned->getRetValue());
    const auto id = node("return", nullptr, returned->getReturnLoc(),
                         blockFor(returned, function_, summary_.blocks));
    summary_.returns.push_back(id);
    for (const auto source : values)
      builder_.edge(source, id, "value");
    return;
  }
  for (const auto *child : stmt->children())
    statement(child);
}

Summary Collector::collect() {
  for (const auto *parameter : function_.decl->parameters()) {
    const auto id = node("parameter", parameter, parameter->getLocation(),
                         summary_.blocks.entry);
    summary_.parameters[parameter] = id;
    remember(id, parameter);
    builder_.event(id, function_, parameter, Access::Write,
                   summary_.blocks.entry);
    if (parameter->getType()->isPointerType()) {
      const auto memory =
          node("parameter-pointee", parameter, parameter->getLocation(),
               summary_.blocks.entry, {}, true);
      summary_.memoryParameters[parameter] = memory;
      remember(memory, parameter);
      builder_.event(memory, function_, parameter, Access::Write,
                     summary_.blocks.entry, -1, true);
    }
  }
  statement(function_.decl->getBody());
  return std::move(summary_);
}

Summary collectSummary(const Function &function, Builder &builder,
                       unsigned depth) {
  return Collector(function, builder, depth).collect();
}

} // namespace facts::variableflow::detail
