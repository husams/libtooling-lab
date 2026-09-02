#include "commands/match/ArgumentOutput.h"

#include <clang/AST/APValue.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>
#include <clang/Lex/Lexer.h>
#include <llvm/Support/raw_ostream.h>

namespace facts::commands::match {
namespace {
std::string sourceText(const clang::Expr &expression,
                       clang::ASTContext &context) {
  const auto range =
      clang::CharSourceRange::getTokenRange(expression.getSourceRange());
  return clang::Lexer::getSourceText(range, context.getSourceManager(),
                                     context.getLangOpts())
      .str();
}

const char *valueCategory(const clang::Expr &expression) {
  if (expression.isLValue())
    return "lvalue";
  if (expression.isXValue())
    return "xvalue";
  return "prvalue";
}

std::string value(const clang::Expr &expression, clang::ASTContext &context) {
  clang::Expr::EvalResult result;
  if (!expression.EvaluateAsRValue(result, context))
    return "unknown";
  std::string output;
  llvm::raw_string_ostream stream(output);
  result.Val.printPretty(stream, context, expression.getType());
  return output;
}
} // namespace

void printArguments(const clang::CallExpr &call, clang::ASTContext &context) {
  for (unsigned index = 0; index < call.getNumArgs(); ++index) {
    const auto &argument = *call.getArg(index);
    llvm::outs() << "argument index=" << index << " source='"
                 << sourceText(argument, context) << "' type='"
                 << argument.getType().getCanonicalType().getAsString()
                 << "' category=" << valueCategory(argument) << " value='"
                 << value(argument, context) << "'\n";
  }
}

} // namespace facts::commands::match
