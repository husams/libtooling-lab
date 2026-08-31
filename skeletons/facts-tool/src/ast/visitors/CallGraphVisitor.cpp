#include "ast/visitors/CallGraphVisitor.h"

#include "analysis/callgraph/CallGraphLinker.h"
#include "analysis/callgraph/DispatchResolver.h"
#include "ast/StoreExtracted.h"
#include "ast/extractors/CallSite.h"
#include "ast/extractors/OverrideRelation.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>
#include <clang/Analysis/CallGraph.h>

namespace facts {

IndexingResult CallGraphVisitor::run() {
  clang::CallGraph graph;
  graph.addToCallGraph(context_.getTranslationUnitDecl());
  callgraph::CallGraphFacts facts;
  for (const auto &entry : graph) {
    const auto *node = entry.second.get();
    if (node == graph.getRoot())
      continue;
    const auto *caller =
        llvm::dyn_cast_or_null<clang::FunctionDecl>(node->getDecl());
    if (!caller)
      continue;
    if (const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(caller)) {
      auto overrides = extractOverrideRelations(
          *method, context_.getSourceManager(), files_, store_);
      if (!overrides)
        return std::unexpected(
            IndexingError{"cannot extract override relation: " +
                          std::string{extractionErrorName(overrides.error())}});
      facts.overrides.insert(facts.overrides.end(), overrides->begin(),
                             overrides->end());
    }
    for (const auto &[calleeNode, siteExpr] : node->callees()) {
      const auto *call = llvm::dyn_cast_or_null<clang::CallExpr>(siteExpr);
      const auto *callee = calleeNode
                               ? llvm::dyn_cast_or_null<clang::FunctionDecl>(
                                     calleeNode->getDecl())
                           : call ? call->getDirectCallee()
                                  : nullptr;
      if (!callee || !call)
        continue;
      auto fact = extractCallSite(*caller, *callee, *call,
                                  context_.getSourceManager(), files_, store_);
      if (!fact)
        return std::unexpected(
            IndexingError{"cannot extract call site: " +
                          std::string{extractionErrorName(fact.error())}});
      if (*fact)
        facts.calls.push_back(std::move(**fact));
    }
  }
  facts.dispatches =
      callgraph::resolveDispatchCalls(facts.calls, facts.overrides);
  return callgraph::linkCallGraphFacts(std::move(facts), store_);
}

} // namespace facts
