#pragma once

#include "cli/Options.h"

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include <optional>
#include <string>

namespace facts {
class FactStore;
class FileManager;
} // namespace facts

namespace facts::commands::match {

class MatchCallback final
    : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  MatchCallback(const cli::MatchOptions &options, FileManager &files,
                FactStore &store);
  void
  run(const clang::ast_matchers::MatchFinder::MatchResult &result) override;

  const std::optional<std::string> &error() const { return error_; }

private:
  const cli::MatchOptions &options_;
  FileManager &files_;
  FactStore &store_;
  std::optional<std::string> error_;
};

} // namespace facts::commands::match
