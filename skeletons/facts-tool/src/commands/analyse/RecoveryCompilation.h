#pragma once
#include "commands/analyse/CallGraphRecoveryInternal.h"
#include <clang/Tooling/CompilationDatabase.h>

namespace facts::commands {
// Freeze the command shared by the attempt key, report, probe and extractor.
class RecoveryCompilation final : public clang::tooling::CompilationDatabase {
public:
  explicit RecoveryCompilation(clang::tooling::CompileCommand command)
      : command_(std::move(command)) {}

  explicit RecoveryCompilation(const RecoveryCandidate &candidate)
      : command_(candidate.entry.workingDirectory, candidate.source.string(),
                 candidate.entry.arguments, "") {}

  std::vector<clang::tooling::CompileCommand>
  getCompileCommands(llvm::StringRef) const override {
    return {command_};
  }

  std::vector<std::string> getAllFiles() const override {
    return {command_.Filename};
  }

  std::vector<clang::tooling::CompileCommand>
  getAllCompileCommands() const override {
    return {command_};
  }

private:
  clang::tooling::CompileCommand command_;
};
} // namespace facts::commands
