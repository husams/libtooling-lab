#include "ast/visitors/BodyVisitor.h"

#include "analysis/callgraph/CallGraphLinker.h"
#include "ast/StoreExtracted.h"
#include "ast/extractors/CallableSite.h"
#include "ast/extractors/DestructorCalls.h"
#include "ast/extractors/UnsupportedSemantics.h"
#include "storage/FactStore.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/Basic/SourceManager.h>

#include <iterator>
#include <ranges>
#include <string>

namespace facts {

void BodyVisitor::captureInvocation(
    ExtractionResult<std::optional<callgraph::CallFact>> fact) {
  if (!fact) {
    status_.record(std::unexpected(
        IndexingError{"cannot extract callable invocation: " +
                      std::string{extractionErrorName(fact.error())}}));
  } else if (*fact) {
    invocationFacts_.push_back(std::move(**fact));
  }
}

bool BodyVisitor::VisitCallExpr(clang::CallExpr *expression) {
  const auto &sources = context_.getSourceManager();
  if (!expression->getDirectCallee() && !expression->isTypeDependent() &&
      !expression->isValueDependent() &&
      sources.isWrittenInMainFile(expression->getExprLoc()))
    reportUnsupportedSemantic("indirect-call", expression->getExprLoc(),
                              sources);
  return true;
}

bool BodyVisitor::VisitCXXConstructExpr(clang::CXXConstructExpr *expression) {
  const auto *callee = expression->getConstructor();
  captureInvocation(extractCallableSite(
      owner_, *callee, expression->getExprLoc(), {}, callee->isImplicit(),
      context_.getSourceManager(), files_, store_));
  return true;
}

bool BodyVisitor::traverse(clang::Stmt *body) {
  if (!TraverseStmt(body))
    return false;
  const auto *constructor = llvm::dyn_cast<clang::CXXConstructorDecl>(&owner_);
  return !constructor ||
         std::ranges::all_of(constructor->inits(),
                             [&](const auto *initializer) {
                               return TraverseStmt(initializer->getInit());
                             });
}

IndexingResult BodyVisitor::persistInvocations() {
  auto destructors = extractDestructorCalls(owner_, context_, files_, store_);
  if (!destructors)
    return std::unexpected(
        IndexingError{"cannot extract destructor invocation: " +
                      std::string{extractionErrorName(destructors.error())}});
  invocationFacts_.insert(invocationFacts_.end(),
                          std::make_move_iterator(destructors->begin()),
                          std::make_move_iterator(destructors->end()));
  return callgraph::linkCallGraphFacts(
      callgraph::CallGraphFacts{std::move(invocationFacts_), {}, {}}, store_);
}

} // namespace facts
