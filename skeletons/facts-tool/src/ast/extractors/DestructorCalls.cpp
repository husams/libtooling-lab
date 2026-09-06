#include "ast/extractors/DestructorCalls.h"

#include "ast/extractors/CallableSite.h"
#include "ast/extractors/UnsupportedSemantics.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/Analysis/CFG.h>

namespace facts {
namespace {

clang::SourceLocation site(const clang::CFGElement &element,
                           const clang::FunctionDecl &caller) {
  if (const auto value = element.getAs<clang::CFGAutomaticObjDtor>()) {
    if (const auto *trigger = value->getTriggerStmt())
      return trigger->getEndLoc();
    return value->getVarDecl()->getLocation();
  }
  if (const auto value = element.getAs<clang::CFGDeleteDtor>())
    return value->getDeleteExpr()->getExprLoc();
  if (const auto value = element.getAs<clang::CFGTemporaryDtor>())
    return value->getBindTemporaryExpr()->getExprLoc();
  if (const auto value = element.getAs<clang::CFGMemberDtor>())
    return value->getFieldDecl()->getLocation();
  if (const auto value = element.getAs<clang::CFGBaseDtor>())
    return value->getBaseSpecifier()->getBeginLoc();
  return caller.getEndLoc();
}

} // namespace

ExtractionResult<std::vector<callgraph::CallFact>>
extractDestructorCalls(const clang::FunctionDecl &caller,
                       clang::ASTContext &context, FileManager &files,
                       FactStore &store) {
  clang::CFG::BuildOptions options;
  options.AddImplicitDtors = true;
  options.AddTemporaryDtors = true;
  auto graph = clang::CFG::buildCFG(
      &caller, const_cast<clang::Stmt *>(caller.getBody()), &context, options);
  if (!graph) {
    reportUnsupportedSemantic("implicit-cleanup", caller.getLocation(),
                              context.getSourceManager());
    return std::vector<callgraph::CallFact>{};
  }
  std::vector<callgraph::CallFact> facts;
  for (const auto *block : *graph)
    for (const auto &element : *block)
      if (const auto dtor = element.getAs<clang::CFGImplicitDtor>()) {
        const auto *callee = dtor->getDestructorDecl(context);
        if (!callee) {
          reportUnsupportedSemantic("implicit-cleanup", site(element, caller),
                                    context.getSourceManager());
          continue;
        }
        auto fact =
            extractCallableSite(caller, *callee, site(element, caller), {},
                                true, context.getSourceManager(), files, store);
        if (!fact)
          return std::unexpected(fact.error());
        if (*fact)
          facts.push_back(std::move(**fact));
      }
  return facts;
}

} // namespace facts
