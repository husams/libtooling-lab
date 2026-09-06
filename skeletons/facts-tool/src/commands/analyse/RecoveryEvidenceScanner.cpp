#include "commands/analyse/RecoveryEvidenceScanner.h"

#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryCompilation.h"
#include "commands/analyse/RecoveryEvidenceScannerCallback.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/Tooling.h"

#include <set>
#include <vector>

namespace facts::commands {
std::expected<RecoveryBodyFacts, std::string>
scanRecoveryBody(const RecoveryCandidate &candidate) {
  if (candidate.entry.arguments.empty())
    return std::unexpected("missing effective recovery command");
  RecoveryBodyFacts facts;
  const std::set<std::string> wanted(candidate.entry.relatedUsrs.begin(),
                                     candidate.entry.relatedUsrs.end());
  RecoveryCompilation database(candidate);
  const std::vector<std::string> sources{candidate.source.string()};
  clang::tooling::ClangTool tool(database, sources);
  tool.clearArgumentsAdjusters();
  RecoveryEvidenceCallback callback(facts, wanted);
  using namespace clang::ast_matchers;
  MatchFinder finder;
  finder.addMatcher(
      functionDecl(isDefinition(), forEachDescendant(callExpr().bind("site")))
          .bind("owner"),
      &callback);
  finder.addMatcher(functionDecl(isDefinition()).bind("definition"), &callback);
  finder.addMatcher(
      functionDecl(isDefinition(),
                   forEachDescendant(stmt(anyOf(cxxConstructExpr(),
                                                cxxNewExpr(), cxxDeleteExpr()))
                                         .bind("unsupported")))
          .bind("owner"),
      &callback);
  if (tool.run(clang::tooling::newFrontendActionFactory(&finder).get()) != 0)
    return std::unexpected("evidence validation compiler failure");
  return facts;
}
} // namespace facts::commands
