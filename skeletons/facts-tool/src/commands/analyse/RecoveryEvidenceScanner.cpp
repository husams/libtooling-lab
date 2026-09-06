#include "commands/analyse/RecoveryEvidenceScanner.h"
#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryEvidenceScannerCallback.h"
#include "commands/analyse/RecoveryScan.h"
#include <clang/ASTMatchers/ASTMatchFinder.h>

namespace facts::commands {
void collectRecoveryScanBodies(const RecoveryCandidate &candidate,
                               RecoveryScan &scan) {
  scan.facts = {};
  const std::set<std::string> wanted(candidate.entry.relatedUsrs.begin(),
                                     candidate.entry.relatedUsrs.end());
  RecoveryEvidenceCallback callback(scan.facts, wanted);
  clang::ast_matchers::MatchFinder finder;
  finder.addMatcher(
      clang::ast_matchers::functionDecl(clang::ast_matchers::isDefinition())
          .bind("definition"),
      &callback);
  for (const auto &unit : scan.units)
    finder.matchAST(unit->getASTContext());
}

std::expected<RecoveryBodyFacts, std::string>
scanRecoveryBody(const RecoveryContext &context,
                 const RecoveryCandidate &candidate) {
  const auto found = context.scans.find(candidate.entry.tuFileId);
  if (found == context.scans.end() || found->second->status != 0)
    return std::unexpected("evidence validation compiler failure");
  return found->second->facts;
}
} // namespace facts::commands
