#pragma once

#include "analysis/variableflow/Summary.h"

namespace facts::variableflow::detail {

struct Storage {
  const clang::VarDecl *variable = nullptr;
  bool memory = false;
};

class Collector {
public:
  Collector(const Function &, Builder &, unsigned depth);
  Summary collect();

private:
  std::int64_t node(std::string kind, const clang::VarDecl *,
                    clang::SourceLocation, int block,
                    const std::string &name = {}, bool memory = false);
  std::int64_t access(Storage, const clang::Stmt *, clang::SourceLocation,
                      Access);
  void remember(std::int64_t, const clang::VarDecl *);
  void dependencies(const std::vector<std::int64_t> &, std::int64_t);
  std::vector<std::int64_t> expression(const clang::Expr *);
  std::vector<std::int64_t> call(const clang::CallExpr *);
  std::vector<std::int64_t> assignment(const clang::BinaryOperator *);
  std::vector<std::int64_t> unary(const clang::UnaryOperator *);
  std::vector<std::int64_t> unsupported(const clang::Expr *, std::string,
                                        std::vector<std::int64_t>);
  void declaration(const clang::VarDecl *, const clang::DeclStmt *);
  void statement(const clang::Stmt *);
  Storage storage(const clang::Expr *) const;
  Storage pointed(const clang::VarDecl *) const;
  const clang::VarDecl *canonical(const clang::VarDecl *) const;
  void pointerBinding(const clang::VarDecl *, const clang::Expr *);

  const Function &function_;
  Builder &builder_;
  Summary summary_;
  std::unordered_map<const clang::VarDecl *, const clang::VarDecl *>
      references_;
  std::unordered_map<const clang::VarDecl *, Storage> pointers_;
};

} // namespace facts::variableflow::detail
