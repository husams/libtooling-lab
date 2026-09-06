#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryCompilation.h"

#include "ast/extractors/NamedDecl.h"
#include "platform/PlatformFlags.h"
#include "tooling/StoredCompilationDatabase.h"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

namespace facts::commands {
namespace {
class ProbeCallback final
    : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  ProbeCallback(const std::set<std::string> &wanted,
                std::set<std::string> &matched)
      : wanted_(wanted), matched_(matched) {}

  void
  run(const clang::ast_matchers::MatchFinder::MatchResult &result) override {
    const auto *function =
        result.Nodes.getNodeAs<clang::FunctionDecl>("symbol");
    if (!function)
      return;
    if (auto usr = extractUsr(*function); usr && wanted_.contains(*usr))
      matched_.insert(*usr);
  }

private:
  const std::set<std::string> &wanted_;
  std::set<std::string> &matched_;
};

} // namespace

std::expected<RecoveryProbeResult, std::string>
probeRecoveryCandidate(const RecoveryContext &context,
                       const RecoveryCandidate &candidate) {
  const std::vector<std::string> sources{candidate.source.string()};
  if (candidate.entry.arguments.empty())
    return std::unexpected(candidate.entry.reason);
  RecoveryCompilation database(candidate);
  std::set<std::string> wanted(candidate.entry.relatedUsrs.begin(),
                               candidate.entry.relatedUsrs.end());
  std::set<std::string> matched;
  clang::tooling::ClangTool tool(database, sources);
  tool.clearArgumentsAdjusters();
  ProbeCallback callback(wanted, matched);
  clang::ast_matchers::MatchFinder finder;
  finder.addMatcher(
      clang::ast_matchers::functionDecl(clang::ast_matchers::isDefinition())
          .bind("symbol"),
      &callback);
  const int status =
      tool.run(clang::tooling::newFrontendActionFactory(&finder).get());
  return RecoveryProbeResult{status, std::move(matched)};
}
} // namespace facts::commands
