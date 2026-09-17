#include "ast/extractors/IndirectCallTarget.h"

#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <llvm/Support/Casting.h>

namespace facts {
namespace {

bool isValueRead(const IndirectCallContext &context) {
  // The final entry is the DeclRefExpr itself. Parentheses preserve the value
  // category; only an actual lvalue-to-rvalue conversion proves a copy read.
  for (auto index = context.ancestors.size(); index > 1; --index) {
    const auto *parent = context.ancestors[index - 2];
    if (llvm::isa<clang::ParenExpr>(parent))
      continue;
    const auto *cast = llvm::dyn_cast<clang::ImplicitCastExpr>(parent);
    return cast && cast->getCastKind() == clang::CK_LValueToRValue;
  }
  return false;
}

const clang::VarDecl *calledVariable(const clang::CallExpr &call) {
  const auto *callee = call.getCallee()->IgnoreParenImpCasts();
  if (const auto *unary = llvm::dyn_cast<clang::UnaryOperator>(callee);
      unary && unary->getOpcode() == clang::UO_Deref)
    callee = unary->getSubExpr()->IgnoreParenImpCasts();
  const auto *reference = llvm::dyn_cast<clang::DeclRefExpr>(callee);
  return reference ? llvm::dyn_cast<clang::VarDecl>(reference->getDecl())
                   : nullptr;
}

const clang::FunctionDecl *initializerTarget(const clang::VarDecl &variable) {
  const auto *initializer = variable.getInit()->IgnoreParenImpCasts();
  if (const auto *unary = llvm::dyn_cast<clang::UnaryOperator>(initializer);
      unary && unary->getOpcode() == clang::UO_AddrOf)
    initializer = unary->getSubExpr()->IgnoreParenImpCasts();
  const auto *reference = llvm::dyn_cast<clang::DeclRefExpr>(initializer);
  return reference ? llvm::dyn_cast<clang::FunctionDecl>(reference->getDecl())
                   : nullptr;
}

} // namespace

void observeIndirectReference(const clang::DeclRefExpr &reference,
                              IndirectCallContext &context) {
  const auto *variable = llvm::dyn_cast<clang::VarDecl>(reference.getDecl());
  if (variable && variable->getType()->isFunctionPointerType() &&
      !isValueRead(context))
    context.unsafeVariables.insert(variable->getCanonicalDecl());
}

const clang::FunctionDecl *
extractIndirectCallTarget(const clang::CallExpr &call,
                          const clang::FunctionDecl &owner,
                          const IndirectCallContext &context) {
  const auto *variable = calledVariable(call);
  if (!variable || !variable->isLocalVarDecl() ||
      variable->getStorageDuration() != clang::SD_Automatic ||
      variable->getDeclContext() != &owner ||
      !variable->getType()->isFunctionPointerType() ||
      variable->getType().isVolatileQualified() || !variable->hasInit() ||
      context.unsafeVariables.contains(variable->getCanonicalDecl()))
    return nullptr;
  return initializerTarget(*variable);
}

} // namespace facts
