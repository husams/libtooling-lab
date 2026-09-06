#include "commands/analyse/RecoveryEvidenceBody.h"

#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/Reference.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"

namespace facts::commands {
void appendRecoveryCall(RecoveryBodyFacts &facts, std::string_view owner,
                        const clang::FunctionDecl &callee,
                        clang::SourceLocation source, bool implicit,
                        const clang::SourceManager &manager) {
  const auto location = extractLocation(manager, source);
  if (!location)
    return;
  auto destination = extractUsr(referenceOwner(callee));
  if (!destination)
    return;
  facts.calls[std::string(owner)].push_back(
      {*destination, manager.getFilename(manager.getExpansionLoc(source)).str(),
       location->offset, location->line, location->column, implicit});
}

namespace {
class Visitor final : public clang::RecursiveASTVisitor<Visitor> {
public:
  Visitor(const clang::SourceManager &manager, std::string_view usr,
          RecoveryBodyFacts &facts)
      : manager_(manager), usr_(usr), facts_(facts) {}

  bool VisitCallExpr(clang::CallExpr *call) {
    const auto *callee = call->getDirectCallee();
    if (!callee) {
      ++facts_.unresolved[std::string(usr_)];
      return true;
    }
    appendRecoveryCall(facts_, usr_, *callee, call->getExprLoc(), false,
                       manager_);
    return true;
  }

  bool VisitCXXConstructExpr(clang::CXXConstructExpr *expression) {
    appendRecoveryCall(facts_, usr_, *expression->getConstructor(),
                       expression->getExprLoc(), false, manager_);
    return true;
  }

  bool TraverseLambdaExpr(clang::LambdaExpr *lambda) {
    if (!WalkUpFromLambdaExpr(lambda))
      return false;
    for (auto *init : lambda->capture_inits())
      if (init && !TraverseStmt(init))
        return false;
    return true;
  }

  bool TraverseDecl(clang::Decl *decl) {
    return llvm::isa_and_nonnull<clang::FunctionDecl>(decl) ||
           clang::RecursiveASTVisitor<Visitor>::TraverseDecl(decl);
  }

private:
  const clang::SourceManager &manager_;
  std::string_view usr_;
  RecoveryBodyFacts &facts_;
};
} // namespace

bool collectRecoveryBodyEvidence(const clang::FunctionDecl &owner,
                                 clang::ASTContext &context,
                                 const clang::SourceManager &manager,
                                 std::string_view usr,
                                 RecoveryBodyFacts &facts) {
  Visitor visitor(manager, usr, facts);
  if (!visitor.TraverseStmt(const_cast<clang::Stmt *>(owner.getBody()))) {
    facts.unsupported = true;
    return false;
  }
  if (const auto *constructor =
          llvm::dyn_cast<clang::CXXConstructorDecl>(&owner))
    for (const auto *initializer : constructor->inits())
      if (!visitor.TraverseStmt(
              const_cast<clang::Expr *>(initializer->getInit()))) {
        facts.unsupported = true;
        return false;
      }
  collectRecoveryDestructorEvidence(owner, context, manager, usr, facts);
  return !facts.unsupported;
}
} // namespace facts::commands
