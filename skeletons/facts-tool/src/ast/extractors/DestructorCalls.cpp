#include "ast/extractors/DestructorCalls.h"

#include "ast/extractors/CallableSite.h"
#include "ast/extractors/ReceiverContext.h"
#include "ast/extractors/UnsupportedSemantics.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
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
  return caller.getEndLoc();
}

const clang::CXXRecordDecl *subobjectRecord(const clang::CFGElement &element) {
  if (const auto value = element.getAs<clang::CFGMemberDtor>())
    return value->getFieldDecl()->getType()->getAsCXXRecordDecl();
  if (const auto value = element.getAs<clang::CFGBaseDtor>())
    return value->getBaseSpecifier()->getType()->getAsCXXRecordDecl();
  return nullptr;
}

const clang::CXXDestructorDecl *destructor(const clang::CFGElement &element,
                                           clang::ASTContext &context) {
  if (const auto *record = subobjectRecord(element))
    return record->getDestructor();
  if (const auto value = element.getAs<clang::CFGImplicitDtor>())
    return value->getDestructorDecl(context);
  return nullptr;
}

ReceiverCertainty certainty(const clang::CFGElement &element) {
  return element.getAs<clang::CFGDeleteDtor>() ? ReceiverCertainty::Possible
                                               : ReceiverCertainty::Exact;
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
                              context.getSourceManager(), files, store);
    return std::vector<callgraph::CallFact>{};
  }
  std::vector<callgraph::CallFact> facts;
  for (const auto *block : *graph)
    for (const auto &element : *block)
      if (element.getAs<clang::CFGImplicitDtor>()) {
        const auto *callee = destructor(element, context);
        if (!callee) {
          reportUnsupportedSemantic("implicit-cleanup", site(element, caller),
                                    context.getSourceManager(), files, store);
          continue;
        }
        auto receiver =
            extractReceiverContext(*callee->getParent(), certainty(element),
                                   context.getSourceManager(), files, store);
        if (!receiver)
          return std::unexpected(receiver.error());
        auto fact = extractCallableSite(
            caller, *callee, site(element, caller), *receiver, true,
            context.getSourceManager(), files, store);
        if (!fact)
          return std::unexpected(fact.error());
        if (*fact)
          facts.push_back(std::move(**fact));
      }
  return facts;
}

} // namespace facts
