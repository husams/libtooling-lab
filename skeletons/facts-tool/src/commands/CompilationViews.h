#pragma once
#include "commands/CompilationDatabase.h"

namespace facts::commands {
// Two immutable views share one parsed JSON/fixed compilation database.
class SharedCompilationView final : public clang::tooling::CompilationDatabase {
public:
  explicit SharedCompilationView(std::shared_ptr<CompilationDatabase> base)
      : base_(std::move(base)) {}
  std::vector<clang::tooling::CompileCommand>
  getCompileCommands(llvm::StringRef file) const override {
    return base_->getCompileCommands(file);
  }
  std::vector<std::string> getAllFiles() const override { return base_->getAllFiles(); }
  std::vector<clang::tooling::CompileCommand> getAllCompileCommands() const override {
    return base_->getAllCompileCommands();
  }
private:
  std::shared_ptr<CompilationDatabase> base_;
};
struct CompilationViews {
  CompilationDatabasePtr stored;
  CompilationDatabasePtr applied;
};
inline CompilationViews compilationViews(CompilationDatabasePtr database,
    const std::vector<std::string> &defaults,
    const std::vector<std::string> &explicitArguments,
    bool explicitProvided) {
  std::shared_ptr<clang::tooling::CompilationDatabase> base = std::move(database);
  const auto &runtime = explicitProvided ? explicitArguments : defaults;
  return {appendExtraArguments(std::make_unique<SharedCompilationView>(base),
                               explicitArguments),
          appendExtraArguments(std::make_unique<SharedCompilationView>(base), runtime)};
}
} // namespace facts::commands
