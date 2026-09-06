#include "commands/analyse/RecoveryEvidenceScannerCallback.h"

#include "ast/extractors/NamedDecl.h"
#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "clang/AST/Expr.h"
#include "clang/Lex/Lexer.h"

#include <optional>

namespace facts::commands {
RecoveryEvidenceCallback::RecoveryEvidenceCallback(
    RecoveryBodyFacts &facts, const std::set<std::string> &wanted)
    : facts_(facts), wanted_(wanted) {}

void RecoveryEvidenceCallback::run(
    const clang::ast_matchers::MatchFinder::MatchResult &result) {
  const auto *definition =
      result.Nodes.getNodeAs<clang::FunctionDecl>("definition");
  if (definition) {
    auto usr = extractUsr(*definition);
    if (usr && wanted_.contains(*usr))
      addDefinition(*definition, result.SourceManager);
  }
  const auto *owner = result.Nodes.getNodeAs<clang::FunctionDecl>("owner");
  if (!owner)
    return;
  const auto ownerUsr = extractUsr(*owner);
  if (!ownerUsr || !wanted_.contains(*ownerUsr))
    return;
  if (result.Nodes.getNodeAs<clang::Stmt>("unsupported")) {
    facts_.unsupported = true;
    return;
  }
  const auto *site = result.Nodes.getNodeAs<clang::CallExpr>("site");
  if (!site)
    return;
  const auto *callee = site->getDirectCallee();
  if (!callee) {
    facts_.unsupported = true;
    return;
  }
  auto destination = extractUsr(*callee);
  if (!destination || !result.SourceManager) {
    facts_.unsupported = true;
    return;
  }
  const auto location =
      result.SourceManager->getExpansionLoc(site->getExprLoc());
  const auto presumed = result.SourceManager->getPresumedLoc(location);
  if (location.isInvalid() || presumed.isInvalid()) {
    facts_.unsupported = true;
    return;
  }
  facts_.calls[*ownerUsr].push_back(
      {*destination, result.SourceManager->getFilename(location).str(),
       result.SourceManager->getFileOffset(location), presumed.getLine(),
       presumed.getColumn()});
}

void RecoveryEvidenceCallback::addDefinition(
    const clang::FunctionDecl &definition,
    const clang::SourceManager *manager) {
  auto usr = extractUsr(definition);
  if (!usr || !manager) {
    facts_.unsupported = true;
    return;
  }
  auto begin = manager->getExpansionLoc(definition.getBeginLoc());
  auto end = clang::Lexer::getLocForEndOfToken(
      manager->getExpansionLoc(definition.getEndLoc()), 0, *manager,
      definition.getASTContext().getLangOpts());
  if (begin.isInvalid() || end.isInvalid() ||
      manager->isInSystemHeader(begin) ||
      manager->getFileID(begin) != manager->getFileID(end)) {
    facts_.unsupported = true;
    return;
  }
  const auto offset = manager->getFileOffset(begin);
  facts_.definitions[*usr] = {manager->getFilename(begin).str(), offset,
                              manager->getFileOffset(end) - offset};
  facts_.calls.try_emplace(*usr);
}
} // namespace facts::commands
