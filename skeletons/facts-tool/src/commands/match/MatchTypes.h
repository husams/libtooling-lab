#pragma once

#include "model/Relation.h"

#include <clang/AST/ASTTypeTraits.h>

#include <string>
#include <variant>
#include <vector>

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

struct NodeMatch {
  std::string binding;
  clang::DynTypedNode node;
};

struct BindingNames {
  std::string source = "source";
  std::string target = "target";
  std::string site = "site";
  std::string call = "call";
  std::string callee = "callee";
};

using Contract =
    std::variant<SymbolMatch, ExpressionMatch, RelationMatch, DirectCallMatch,
                 NodeMatch>;
using Contracts = std::vector<Contract>;

} // namespace facts::commands::match
