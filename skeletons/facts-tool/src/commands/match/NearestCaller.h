#pragma once

#include <expected>
#include <string>

namespace clang {
class ASTContext;
class CallExpr;
class FunctionDecl;
} // namespace clang

namespace facts::commands::match {
std::expected<const clang::FunctionDecl *, std::string>
nearestCaller(const clang::CallExpr &call, clang::ASTContext &context);
}
