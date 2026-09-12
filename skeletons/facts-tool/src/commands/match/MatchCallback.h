#pragma once

#include "cli/Options.h"
#include "model/MatchedSymbol.h"
#include "commands/match/ExpressionEvidence.h"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <llvm/Support/JSON.h>

#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace facts {
class FactStore;
class FileManager;
} // namespace facts

namespace facts::commands::match {

class MatchCallback final
    : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  MatchCallback(const cli::MatchOptions &options, FileManager &files,
                FactStore &store, bool rejectLegacyWrites = false);
  void onStartOfTranslationUnit() override { fingerprints_.clear(); }
  void
  run(const clang::ast_matchers::MatchFinder::MatchResult &result) override;

  const std::optional<std::string> &error() const { return error_; }

  const std::vector<MatchedSymbol> &matchedSymbols() const { return matches_; }
  void writeResults(const std::vector<std::string> &sources,
                    std::ostream &output);

private:
  const cli::MatchOptions &options_;
  FileManager &files_;
  FactStore &store_;
  bool rejectLegacyWrites_ = false;
  std::optional<std::string> error_;
  std::vector<MatchedSymbol> matches_;
  llvm::json::Array results_;
  std::ostringstream text_;
  SourceFingerprintCache fingerprints_;
};

} // namespace facts::commands::match
