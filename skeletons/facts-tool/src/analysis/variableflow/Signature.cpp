#include "analysis/variableflow/Signature.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>

#include <cctype>

namespace facts::variableflow::detail {
namespace {

bool punctuation(char value) {
  return value == ',' || value == '(' || value == ')' || value == '&' ||
         value == '*' || value == '<' || value == '>' || value == ':' ||
         value == '[' || value == ']';
}

std::string normalized(std::string_view value) {
  std::string result;
  bool pendingSpace = false;
  char quote = 0;
  bool escaped = false;
  for (const char current : value) {
    if (quote != 0) {
      result.push_back(current);
      if (escaped)
        escaped = false;
      else if (current == '\\')
        escaped = true;
      else if (current == quote)
        quote = 0;
      continue;
    }
    if (current == '\'' || current == '"') {
      if (pendingSpace && !punctuation(current) && !punctuation(result.back()))
        result.push_back(' ');
      pendingSpace = false;
      quote = current;
      result.push_back(current);
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(current))) {
      pendingSpace = !result.empty();
      continue;
    }
    if (pendingSpace && !punctuation(current) && !punctuation(result.back()))
      result.push_back(' ');
    pendingSpace = false;
    result.push_back(current);
  }
  if (result.starts_with("::"))
    result.erase(0, 2);
  return result;
}

std::string parameterType(const clang::FunctionDecl &decl, unsigned index,
                          bool canonical) {
  const auto *functionType = decl.getType()->getAs<clang::FunctionProtoType>();
  auto type = canonical && functionType != nullptr
                  ? functionType->getParamType(index).getUnqualifiedType()
                  : decl.getParamDecl(index)->getType();
  if (canonical)
    type = type.getCanonicalType();
  auto policy = decl.getASTContext().getPrintingPolicy();
  policy.SuppressTagKeyword = true;
  return type.getAsString(policy);
}

} // namespace

std::string functionSignature(const clang::FunctionDecl &decl, bool canonical) {
  std::string result = decl.getQualifiedNameAsString() + "(";
  for (unsigned index = 0; index < decl.getNumParams(); ++index) {
    if (index != 0)
      result += ", ";
    result += parameterType(decl, index, canonical);
  }
  if (decl.isVariadic()) {
    if (decl.getNumParams() != 0)
      result += ", ";
    result += "...";
  }
  result += ')';
  if (const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(&decl)) {
    const auto qualifiers = method->getMethodQualifiers();
    if (qualifiers.hasConst())
      result += " const";
    if (qualifiers.hasVolatile())
      result += " volatile";
    if (method->getRefQualifier() == clang::RQ_LValue)
      result += " &";
    if (method->getRefQualifier() == clang::RQ_RValue)
      result += " &&";
  }
  return result;
}

bool matchesFunctionSignature(const clang::FunctionDecl &decl,
                              std::string_view selector) {
  const auto normalizedSelector = normalized(selector);
  return normalized(functionSignature(decl)) == normalizedSelector ||
         normalized(functionSignature(decl, true)) == normalizedSelector;
}

std::string describeFunction(const clang::FunctionDecl &decl,
                             std::string_view usr) {
  return functionSignature(decl) + " [USR " + std::string(usr) + ']';
}

} // namespace facts::variableflow::detail
