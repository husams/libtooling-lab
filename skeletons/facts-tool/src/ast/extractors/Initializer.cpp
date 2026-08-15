#include "ast/extractors/Initializer.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/Lex/Lexer.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>

#include <optional>
#include <string>
#include <utility>

namespace facts {
namespace {

std::string printedExpression(const clang::Expr &expression,
                              const clang::ASTContext &context) {
  std::string text;
  llvm::raw_string_ostream stream(text);
  expression.printPretty(stream, nullptr,
                         clang::PrintingPolicy{context.getLangOpts()});
  return text;
}

std::string initializerExpression(const clang::Expr &expression,
                                  const clang::ASTContext &context,
                                  const clang::SourceManager &sourceManager) {
  bool invalid = false;
  const auto text = clang::Lexer::getSourceText(
      clang::CharSourceRange::getTokenRange(expression.getSourceRange()),
      sourceManager, context.getLangOpts(), &invalid);
  return !invalid && !text.empty() ? text.str()
                                   : printedExpression(expression, context);
}

std::optional<EvaluatedValue>
evaluatedValue(const clang::Expr &expression,
               const clang::QualType &declaredType,
               const clang::ASTContext &context) {
  const auto *valueExpression = expression.IgnoreParenImpCasts();
  if (const auto *literal =
          llvm::dyn_cast<clang::StringLiteral>(valueExpression)) {
    return EvaluatedValue{EvaluatedValueKind::String,
                          literal->getBytes().str()};
  }

  clang::Expr::EvalResult result;
  if (!expression.EvaluateAsRValue(result, context) || !result.Val.hasValue()) {
    return std::nullopt;
  }

  if (result.Val.isInt() && declaredType->isBooleanType()) {
    return EvaluatedValue{EvaluatedValueKind::Boolean,
                          result.Val.getInt().getBoolValue() ? "true"
                                                             : "false"};
  }

  const auto kind = result.Val.isInt() ? EvaluatedValueKind::Integer
                    : result.Val.isFloat()
                        ? EvaluatedValueKind::Floating
                        : std::optional<EvaluatedValueKind>{};
  if (!kind) {
    return std::nullopt;
  }

  std::string text;
  llvm::raw_string_ostream stream(text);
  result.Val.printPretty(stream, context, declaredType);
  if (text.empty()) {
    return std::nullopt;
  }
  return EvaluatedValue{*kind, std::move(text)};
}

} // namespace

std::optional<VariableInitializer>
extractInitializer(const clang::Expr *expression,
                   const clang::QualType &declaredType,
                   const clang::ASTContext &context,
                   const clang::SourceManager &sourceManager) {
  if (expression == nullptr) {
    return std::nullopt;
  }
  return VariableInitializer{
      .expression = initializerExpression(*expression, context, sourceManager),
      .evaluated = evaluatedValue(*expression, declaredType, context),
  };
}

} // namespace facts
