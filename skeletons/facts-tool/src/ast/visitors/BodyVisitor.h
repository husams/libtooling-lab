#ifndef FACTS_TOOL_AST_VISITORS_BODY_VISITOR_H
#define FACTS_TOOL_AST_VISITORS_BODY_VISITOR_H

#include "ast/Indexing.h"
#include "ast/extractors/Reference.h"

#include <clang/AST/RecursiveASTVisitor.h>

#include <unordered_set>
#include <utility>
#include <vector>

namespace facts {
class FactStore;
class FileManager;

class BodyVisitor final : public clang::RecursiveASTVisitor<BodyVisitor> {
public:
  BodyVisitor(const clang::FunctionDecl &owner, clang::ASTContext &context,
              FileManager &files, FactStore &store, IndexingStatus &status)
      : owner_(owner), context_(context), files_(files), store_(store),
        status_(status) {}

  bool TraverseCXXMethodDecl(clang::CXXMethodDecl *decl);
  bool TraverseFunctionDecl(clang::FunctionDecl *decl);
  bool TraverseLambdaExpr(clang::LambdaExpr *expression);

  bool VisitCXXRecordDecl(clang::CXXRecordDecl *decl);
  bool VisitDeclRefExpr(clang::DeclRefExpr *expression);
  bool VisitEnumConstantDecl(clang::EnumConstantDecl *decl);
  bool VisitEnumDecl(clang::EnumDecl *decl);
  bool VisitFieldDecl(clang::FieldDecl *decl);
  bool VisitMemberExpr(clang::MemberExpr *expression);
  bool VisitNamedDecl(clang::NamedDecl *decl);
  bool VisitVarDecl(clang::VarDecl *decl);

  IndexingResult flush();

private:
  using PendingBody = std::pair<const clang::FunctionDecl *, clang::Stmt *>;

  void capture(ExtractionResult<std::optional<UseFact>> fact,
               const clang::NamedDecl &referenced);
  void capture(const clang::Expr &expression,
               const clang::NamedDecl &referenced,
               clang::SourceLocation location);
  void schedule(const clang::FunctionDecl &decl);
  IndexingResult flushNestedBodies();
  IndexingResult persistUses();

  const clang::FunctionDecl &owner_;
  clang::ASTContext &context_;
  FileManager &files_;
  FactStore &store_;
  IndexingStatus &status_;
  std::vector<UseFact> facts_;
  std::vector<PendingBody> pendingBodies_;
  std::unordered_set<const clang::Stmt *> scheduledBodies_;
};

} // namespace facts

#endif // FACTS_TOOL_AST_VISITORS_BODY_VISITOR_H
