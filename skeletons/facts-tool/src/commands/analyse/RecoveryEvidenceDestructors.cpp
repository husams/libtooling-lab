#include "commands/analyse/RecoveryEvidenceBody.h"

#include "ast/extractors/NamedDecl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/Analysis/CFG.h"

namespace facts::commands {
namespace {
clang::SourceLocation destructorSite(const clang::CFGElement &element,
                                     const clang::FunctionDecl &caller) {
  if (const auto value = element.getAs<clang::CFGAutomaticObjDtor>())
    return value->getTriggerStmt() ? value->getTriggerStmt()->getEndLoc()
                                   : value->getVarDecl()->getLocation();
  if (const auto value = element.getAs<clang::CFGDeleteDtor>())
    return value->getDeleteExpr()->getExprLoc();
  if (const auto value = element.getAs<clang::CFGTemporaryDtor>())
    return value->getBindTemporaryExpr()->getExprLoc();
  return caller.getEndLoc();
}

const clang::CXXRecordDecl *subobject(const clang::CFGElement &element) {
  if (const auto value = element.getAs<clang::CFGMemberDtor>())
    return value->getFieldDecl()->getType()->getAsCXXRecordDecl();
  if (const auto value = element.getAs<clang::CFGBaseDtor>())
    return value->getBaseSpecifier()->getType()->getAsCXXRecordDecl();
  return nullptr;
}

const clang::CXXDestructorDecl *destructor(const clang::CFGElement &element,
                                           clang::ASTContext &context) {
  if (const auto *record = subobject(element))
    return record->getDestructor();
  if (const auto value = element.getAs<clang::CFGImplicitDtor>())
    return value->getDestructorDecl(context);
  return nullptr;
}
} // namespace

void collectRecoveryDestructorEvidence(const clang::FunctionDecl &owner,
                                       clang::ASTContext &context,
                                       const clang::SourceManager &,
                                       std::string_view usr,
                                       RecoveryBodyFacts &facts) {
  clang::CFG::BuildOptions options;
  options.AddImplicitDtors = true;
  options.AddTemporaryDtors = true;
  auto graph = clang::CFG::buildCFG(
      &owner, const_cast<clang::Stmt *>(owner.getBody()), &context, options);
  if (!graph) {
    facts.unsupported = true;
    return;
  }
  for (const auto *block : *graph)
    for (const auto &element : *block) {
      if (!element.getAs<clang::CFGImplicitDtor>())
        continue;
      const auto *callee = destructor(element, context);
      if (!callee) {
        facts.unsupported = true;
        continue;
      }
      appendRecoveryCall(facts, usr, *callee, destructorSite(element, owner),
                         true, context.getSourceManager());
    }
}
} // namespace facts::commands
