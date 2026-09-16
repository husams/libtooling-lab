#include "tooling/astcache/Parse.h"
#include "tooling/astcache/RevisionObserver.h"

#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Tooling/Tooling.h>

namespace facts::astcache::detail {
namespace {

class ObservedSyntaxAction final : public clang::SyntaxOnlyAction {
public:
  explicit ObservedSyntaxAction(RevisionObservations *observations)
      : observations_(observations) {}

  bool BeginSourceFileAction(clang::CompilerInstance &compiler) override {
    if (observations_)
      attachRevisionObserver(compiler, compiler.getFileSystemOpts().WorkingDir,
                             *observations_);
    return clang::SyntaxOnlyAction::BeginSourceFileAction(compiler);
  }

private:
  RevisionObservations *observations_;
};

class ASTBuilder final : public clang::tooling::ToolAction {
public:
  ASTBuilder(std::vector<std::unique_ptr<clang::ASTUnit>> &units,
             RevisionObservations *observations)
      : units_(units), observations_(observations) {}

  bool runInvocation(
      std::shared_ptr<clang::CompilerInvocation> invocation,
      clang::FileManager *files,
      std::shared_ptr<clang::PCHContainerOperations> containers,
      clang::DiagnosticConsumer *consumer) override {
    const auto cwd = files->getVirtualFileSystem().getCurrentWorkingDirectory();
    if (!cwd)
      return false;
    invocation->getFileSystemOpts().WorkingDir = *cwd;
    invocation->getPreprocessorOpts().DetailedRecord = true;
    auto diagnostics = clang::CompilerInstance::createDiagnostics(
        files->getVirtualFileSystem(), invocation->getDiagnosticOpts(), consumer,
        false);
    ObservedSyntaxAction action(observations_);
    std::unique_ptr<clang::ASTUnit> unit(
        clang::ASTUnit::LoadFromCompilerInvocationAction(
            std::move(invocation), std::move(containers), nullptr,
            std::move(diagnostics), &action));
    if (!unit)
      return false;
    const bool success = !unit->getDiagnostics().hasErrorOccurred();
    units_.push_back(std::move(unit));
    return success;
  }

private:
  std::vector<std::unique_ptr<clang::ASTUnit>> &units_;
  RevisionObservations *observations_;
};

} // namespace

int parsePersistent(clang::tooling::ClangTool &tool,
                    std::vector<std::unique_ptr<clang::ASTUnit>> &units,
                    RevisionObservations *observations) {
  ASTBuilder builder(units, observations);
  return tool.run(&builder);
}

} // namespace facts::astcache::detail
