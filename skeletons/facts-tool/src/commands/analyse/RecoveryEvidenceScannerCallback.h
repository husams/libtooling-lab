#pragma once

#include "commands/analyse/RecoveryEvidenceScanner.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include <set>
#include <string>

namespace facts::commands {
class RecoveryEvidenceCallback final
    : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  RecoveryEvidenceCallback(RecoveryBodyFacts &, const std::set<std::string> &);
  void run(const clang::ast_matchers::MatchFinder::MatchResult &) override;

private:
  void addDefinition(const clang::FunctionDecl &, const clang::SourceManager *);
  RecoveryBodyFacts &facts_;
  const std::set<std::string> &wanted_;
};
} // namespace facts::commands
