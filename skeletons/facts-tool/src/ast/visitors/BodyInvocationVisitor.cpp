#include "ast/visitors/BodyVisitor.h"

#include "analysis/callgraph/CallGraphLinker.h"
#include "ast/StoreExtracted.h"
#include "ast/extractors/CallableSite.h"
#include "ast/extractors/UnsupportedSemantics.h"
#include "storage/FactStore.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/Basic/SourceManager.h>

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
      !expression->isValueDependent())
    reportUnsupportedSemantic("indirect-call", expression->getExprLoc(),
                              sources, files_, store_);
  return true;
}

bool BodyVisitor::VisitCXXConstructExpr(clang::CXXConstructExpr *expression) {
  const auto *callee = expression->getConstructor();
  captureInvocation(
      extractReceiverContext(*callee->getParent(), ReceiverCertainty::Exact,
                             context_.getSourceManager(), files_, store_)
          .and_then([&](ReceiverContext receiver) {
            return extractCallableSite(
                owner_, *callee, expression->getExprLoc(), receiver, false,
                false, context_.getSourceManager(), files_, store_);
          }));
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
  return callgraph::linkCallGraphFacts(
      callgraph::CallGraphFacts{std::move(invocationFacts_), {}, {}}, store_);
}

} // namespace facts
