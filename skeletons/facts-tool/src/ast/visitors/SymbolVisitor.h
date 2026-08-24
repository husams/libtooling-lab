#ifndef FACTS_TOOL_AST_VISITORS_SYMBOL_VISITOR_H
#define FACTS_TOOL_AST_VISITORS_SYMBOL_VISITOR_H

#include "ast/Indexing.h"

#include <clang/AST/RecursiveASTVisitor.h>

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace facts {
class FactStore;
class FileManager;

class SymbolVisitor final : public clang::RecursiveASTVisitor<SymbolVisitor> {
public:
  using Base = clang::RecursiveASTVisitor<SymbolVisitor>;

  SymbolVisitor(clang::ASTContext &context, FileManager &files,
                FactStore &store, IndexingStatus &status)
      : context_(context), files_(files), store_(store), status_(status) {}

  bool shouldVisitTemplateInstantiations() const { return true; }

  bool TraverseCXXMethodDecl(clang::CXXMethodDecl *decl);
  bool TraverseFieldDecl(clang::FieldDecl *decl);
  bool TraverseTranslationUnitDecl(clang::TranslationUnitDecl *decl);
  bool TraverseParmVarDecl(clang::ParmVarDecl *decl);
  bool TraverseStmt(clang::Stmt *statement);
  bool TraverseUsingDirectiveDecl(clang::UsingDirectiveDecl *decl);
  bool VisitFunctionDecl(clang::FunctionDecl *decl);
  bool VisitNamedDecl(clang::NamedDecl *decl);

  IndexingResult flushBodies();

private:
  void schedule(const clang::FunctionDecl &decl);

  clang::ASTContext &context_;
  FileManager &files_;
  FactStore &store_;
  IndexingStatus &status_;
  std::vector<std::pair<const clang::FunctionDecl *, clang::Stmt *>>
      pendingBodies_;
  std::unordered_map<const clang::Stmt *, const clang::FunctionDecl *>
      bodyOwners_;
  std::unordered_set<const clang::Stmt *> visitedBodies_;
};

} // namespace facts

#endif // FACTS_TOOL_AST_VISITORS_SYMBOL_VISITOR_H
