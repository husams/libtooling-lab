#pragma once

namespace clang {
class ASTContext;
class CallExpr;
} // namespace clang

namespace facts::commands::match {
void printArguments(const clang::CallExpr &call, clang::ASTContext &context);
}
