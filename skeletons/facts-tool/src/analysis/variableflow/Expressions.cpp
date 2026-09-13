#include "analysis/variableflow/Collector.h"

#include <clang/AST/ExprCXX.h>
#include <clang/Lex/Lexer.h>

namespace facts::variableflow::detail {
namespace {

bool conflicting(const AccessEvent &left, const AccessEvent &right) {
  return left.variableUsr == right.variableUsr &&
         (left.kind != Access::Read || right.kind != Access::Read);
}

} // namespace

std::vector<std::int64_t> Collector::expression(const clang::Expr *expr) {
  expr = strip(expr);
  if (expr == nullptr ||
      llvm::isa<clang::UnaryExprOrTypeTraitExpr, clang::CXXNoexceptExpr>(expr))
    return {};
  if (const auto *reference = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
    const auto target = storage(reference);
    return target.variable == nullptr
               ? std::vector<std::int64_t>{}
               : std::vector{access(target, reference, reference->getExprLoc(),
                                    Access::Read)};
  }
  if (const auto *lambda = llvm::dyn_cast<clang::LambdaExpr>(expr)) {
    std::vector<std::int64_t> captures;
    for (const auto *capture : lambda->capture_inits()) {
      const auto values = expression(capture);
      captures.insert(captures.end(), values.begin(), values.end());
    }
    return unsupported(expr, "lambda-capture", std::move(captures));
  }
  if (const auto *callExpr = llvm::dyn_cast<clang::CallExpr>(expr))
    return call(callExpr);
  if (const auto *binary = llvm::dyn_cast<clang::BinaryOperator>(expr)) {
    if (binary->isAssignmentOp())
      return assignment(binary);
    auto left = expression(binary->getLHS());
    const auto right = expression(binary->getRHS());
    left.insert(left.end(), right.begin(), right.end());
    return left;
  }
  if (const auto *unaryExpr = llvm::dyn_cast<clang::UnaryOperator>(expr))
    return unary(unaryExpr);
  std::vector<std::int64_t> values;
  for (const auto *child : expr->children())
    if (const auto *nested = llvm::dyn_cast_or_null<clang::Expr>(child)) {
      const auto sources = expression(nested);
      values.insert(values.end(), sources.begin(), sources.end());
    }
  if (llvm::isa<clang::MemberExpr, clang::ArraySubscriptExpr,
                clang::CXXConstructExpr, clang::CXXNewExpr,
                clang::CXXDeleteExpr>(expr))
    return unsupported(expr, "unsupported-object", std::move(values));
  if (values.empty()) {
    const auto spelling = clang::Lexer::getSourceText(
        clang::CharSourceRange::getTokenRange(expr->getSourceRange()),
        function_.context->getSourceManager(),
        function_.context->getLangOpts());
    values.push_back(node("literal", nullptr, expr->getExprLoc(),
                          blockFor(expr, function_, summary_.blocks),
                          spelling.str()));
  }
  return values;
}

std::vector<std::int64_t> Collector::call(const clang::CallExpr *callExpr) {
  std::vector<Argument> arguments;
  std::vector<AccessEvent> previous;
  bool unspecifiedOrder = false;
  for (const auto *argument : callExpr->arguments()) {
    auto target = storage(argument);
    const auto *unary = llvm::dyn_cast<clang::UnaryOperator>(strip(argument));
    bool address = unary != nullptr && unary->getOpcode() == clang::UO_AddrOf;
    const auto *callee = callExpr->getDirectCallee();
    const auto index = arguments.size();
    if (!address && target.variable != nullptr && callee != nullptr &&
        index < callee->getNumParams() &&
        callee->getParamDecl(index)->getType()->isPointerType()) {
      target = pointed(target.variable);
      address = !target.memory;
    }
    const auto start = builder_.events.size();
    arguments.push_back(
        Argument{expression(argument), target.variable, address});
    for (auto index = start; index < builder_.events.size(); ++index)
      for (const auto &earlier : previous)
        unspecifiedOrder |= conflicting(builder_.events[index], earlier);
    previous.insert(previous.end(), builder_.events.begin() + start,
                    builder_.events.end());
  }
  const auto *callee = callExpr->getDirectCallee();
  const auto id = node("call", nullptr, callExpr->getExprLoc(),
                       blockFor(callExpr, function_, summary_.blocks),
                       callee ? functionName(*callee) : "indirect");
  if (unspecifiedOrder)
    builder_.boundary(id, "unspecified-evaluation-order",
                      "argument read/write order is not fixed by the language",
                      summary_.depth);
  for (const auto &argument : arguments)
    for (const auto source : argument.sources)
      builder_.edge(source, id, "argument", id);
  if (callee == nullptr)
    for (const auto source : expression(callExpr->getCallee()))
      builder_.edge(source, id, "argument", id);
  if (const auto *member = llvm::dyn_cast<clang::CXXMemberCallExpr>(callExpr)) {
    const auto receiver = expression(member->getImplicitObjectArgument());
    for (const auto source : receiver)
      builder_.edge(source, id, "argument", id);
    if (!receiver.empty())
      builder_.boundary(id, "object-receiver",
                        "implicit object storage is not summarized",
                        summary_.depth);
  }
  summary_.calls.push_back(Call{callExpr, id, std::move(arguments)});
  return {id};
}

} // namespace facts::variableflow::detail
