#include "commands/match/NearestCaller.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Tooling.h>

#include <cstdlib>

using namespace clang;
using namespace clang::ast_matchers;

namespace {
template <typename Value>
void require(Value &&value) {
  if (!static_cast<bool>(value))
    std::abort();
}

template <typename Matcher>
const CallExpr &oneCall(ASTContext &context, const Matcher &matcher) {
  auto found = match(matcher.bind("call"), context);
  require(found.size() == 1);
  return *found.front().template getNodeAs<CallExpr>("call");
}

void expectRejected(const CallExpr &call, ASTContext &context,
                    const char *message) {
  auto caller = facts::commands::match::nearestCaller(call, context);
  require(!caller && caller.error() == message);
}
} // namespace

int main() {
  auto ast = tooling::buildASTFromCodeWithArgs(
      "namespace N { void sink(); int global = (sink(), 0); struct Owner { "
      "int field = (sink(), 0); void method() { sink(); } }; "
      "void defaulted(int value = (sink(), 0)); }",
      {"-std=c++23"});
  require(ast);
  auto &context = ast->getASTContext();
  const auto &inside = oneCall(
      context,
      callExpr(callee(functionDecl(hasName("N::sink"))),
               hasAncestor(cxxMethodDecl(hasName("N::Owner::method")))));
  auto caller = facts::commands::match::nearestCaller(inside, context);
  require(caller &&
          (*caller)->getQualifiedNameAsString() == "N::Owner::method");
  const auto &global =
      oneCall(context, callExpr(callee(functionDecl(hasName("N::sink"))),
                                hasAncestor(varDecl(hasName("N::global")))));
  expectRejected(global, context, "call has no enclosing callable");
  const auto &field = oneCall(
      context, callExpr(callee(functionDecl(hasName("N::sink"))),
                        hasAncestor(fieldDecl(hasName("N::Owner::field")))));
  expectRejected(field, context, "call is not owned by a function body");
  const auto &argument =
      oneCall(context, callExpr(callee(functionDecl(hasName("N::sink"))),
                                hasAncestor(parmVarDecl(hasName("value")))));
  expectRejected(argument, context, "call is not owned by a function body");
}
