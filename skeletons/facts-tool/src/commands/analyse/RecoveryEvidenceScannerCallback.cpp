#include "commands/analyse/RecoveryEvidenceScannerCallback.h"

#include "ast/extractors/NamedDecl.h"
#include "commands/analyse/RecoveryEvidenceBody.h"
#include "clang/Lex/Lexer.h"

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
    if (usr && wanted_.contains(*usr)) {
      addDefinition(*definition, result.SourceManager);
      if (result.Context && result.SourceManager)
        collectRecoveryBodyEvidence(*definition, *result.Context,
                                    *result.SourceManager, *usr, facts_);
    }
  }
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
