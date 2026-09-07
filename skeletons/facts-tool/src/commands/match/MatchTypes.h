#pragma once

#include "model/Relation.h"

#include <variant>

namespace clang {
class CallExpr;
class Expr;
class FunctionDecl;
class NamedDecl;
class Stmt;
} // namespace clang

namespace facts::commands::match {

struct SymbolMatch {
  const clang::NamedDecl &symbol;
};

struct ExpressionMatch {
  const clang::Expr &expression;
};

struct RelationMatch {
  const clang::NamedDecl &source;
  const clang::NamedDecl &target;
  const clang::Stmt *site;
  const clang::NamedDecl *declarationSite;
  RelationKind kind;
};

struct DirectCallMatch {
  const clang::CallExpr &call;
  const clang::FunctionDecl &callee;
};

using Contract =
    std::variant<SymbolMatch, ExpressionMatch, RelationMatch, DirectCallMatch>;

} // namespace facts::commands::match
