#include "analysis/variableflow/Collector.h"

namespace facts::variableflow::detail {

std::vector<std::int64_t>
Collector::unsupported(const clang::Expr *expr, std::string reason,
                       std::vector<std::int64_t> values) {
  const auto id = node("unsupported", nullptr, expr->getExprLoc(),
                       blockFor(expr, function_, summary_.blocks), reason);
  builder_.boundary(id, std::move(reason), expr->getStmtClassName(),
                    summary_.depth);
  dependencies(values, id);
  values.push_back(id);
  return values;
}

std::vector<std::int64_t>
Collector::assignment(const clang::BinaryOperator *binary) {
  auto values = expression(binary->getRHS());
  const auto target = storage(binary->getLHS());
  if (target.variable == nullptr) {
    const auto base = expression(binary->getLHS());
    if (!base.empty()) {
      const auto id = base.back();
      if (builder_.graph.nodes.at(static_cast<std::size_t>(id - 1)).kind ==
          "unsupported") {
        builder_.boundary(id, "unsupported-storage", binary->getStmtClassName(),
                          summary_.depth);
        dependencies(values, id);
        return {id};
      }
    }
    values.insert(values.end(), base.begin(), base.end());
    return unsupported(binary, "unsupported-storage", std::move(values));
  }
  if (const auto *deref =
          llvm::dyn_cast<clang::UnaryOperator>(strip(binary->getLHS()));
      deref != nullptr && deref->getOpcode() == clang::UO_Deref) {
    const auto addresses = expression(deref->getSubExpr());
    values.insert(values.end(), addresses.begin(), addresses.end());
  }
  const auto kind =
      binary->isCompoundAssignmentOp() ? Access::Update : Access::Write;
  const auto id = access(target, binary, binary->getLHS()->getExprLoc(), kind);
  dependencies(values, id);
  if (target.memory && !llvm::isa<clang::ParmVarDecl>(target.variable))
    builder_.boundary(id, "unknown-alias", "pointee identity is not resolved",
                      summary_.depth);
  if (target.variable->getType()->isPointerType() && !target.memory) {
    pointerBinding(target.variable, binary->getRHS());
    builder_.boundary(id, "alias-rebind",
                      "pointer rebinding is not path-sensitive",
                      summary_.depth);
  }
  return {id};
}

std::vector<std::int64_t> Collector::unary(const clang::UnaryOperator *unary) {
  const auto target =
      storage(unary->isIncrementDecrementOp() ? unary->getSubExpr() : unary);
  if (unary->isIncrementDecrementOp() && target.variable != nullptr) {
    const auto id = access(target, unary, unary->getSubExpr()->getExprLoc(),
                           Access::Update);
    if (target.memory && !llvm::isa<clang::ParmVarDecl>(target.variable))
      builder_.boundary(id, "unknown-alias", "pointee identity is not resolved",
                        summary_.depth);
    return {id};
  }
  auto values = expression(unary->getSubExpr());
  if (unary->getOpcode() == clang::UO_Deref &&
      storage(unary->getSubExpr()).memory)
    return unsupported(unary, "unsupported-alias-depth", std::move(values));
  if (unary->getOpcode() == clang::UO_Deref && target.variable != nullptr) {
    const auto id = access(target, unary, unary->getExprLoc(), Access::Read);
    dependencies(values, id);
    if (target.memory && !llvm::isa<clang::ParmVarDecl>(target.variable))
      builder_.boundary(id, "unknown-alias", "pointee identity is not resolved",
                        summary_.depth);
    return {id};
  }
  return values;
}

} // namespace facts::variableflow::detail
