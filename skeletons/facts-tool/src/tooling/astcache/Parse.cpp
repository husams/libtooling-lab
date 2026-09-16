#include "tooling/astcache/Parse.h"

#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Tooling/Tooling.h>

#include <algorithm>

namespace facts::astcache::detail {
namespace {

class LookupObserver final : public clang::PPCallbacks {
public:
  explicit LookupObserver(std::vector<std::string> &names) : names_(names) {}

  void InclusionDirective(clang::SourceLocation, const clang::Token &,
                          llvm::StringRef name, bool, clang::CharSourceRange,
                          clang::OptionalFileEntryRef, llvm::StringRef,
                          llvm::StringRef, const clang::Module *, bool,
                          clang::SrcMgr::CharacteristicKind) override {
    names_.push_back(name.str());
  }

  void HasInclude(clang::SourceLocation, llvm::StringRef name, bool,
                  clang::OptionalFileEntryRef,
                  clang::SrcMgr::CharacteristicKind) override {
    names_.push_back(name.str());
  }

private:
  std::vector<std::string> &names_;
};

class LookupAction final : public clang::SyntaxOnlyAction {
public:
  explicit LookupAction(std::vector<std::string> &names) : names_(names) {}

  bool BeginSourceFileAction(clang::CompilerInstance &compiler) override {
    compiler.getPreprocessor().addPPCallbacks(
        std::make_unique<LookupObserver>(names_));
    return true;
  }

private:
  std::vector<std::string> &names_;
};

class ASTBuilder final : public clang::tooling::ToolAction {
public:
  ASTBuilder(std::vector<std::unique_ptr<clang::ASTUnit>> &units,
             std::vector<std::string> &names)
      : units_(units), names_(names) {}

  bool runInvocation(
      std::shared_ptr<clang::CompilerInvocation> invocation,
      clang::FileManager *files,
      std::shared_ptr<clang::PCHContainerOperations> containers,
      clang::DiagnosticConsumer *consumer) override {
    auto cwd = files->getVirtualFileSystem().getCurrentWorkingDirectory();
    if (!cwd)
      return false;
    invocation->getFileSystemOpts().WorkingDir = *cwd;
    invocation->getPreprocessorOpts().DetailedRecord = true;
    auto diagnostics = clang::CompilerInstance::createDiagnostics(
        files->getVirtualFileSystem(), invocation->getDiagnosticOpts(), consumer,
        false);
    LookupAction action(names_);
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
  std::vector<std::string> &names_;
};

} // namespace

int parseWithLookups(clang::tooling::ClangTool &tool,
                     std::vector<std::unique_ptr<clang::ASTUnit>> &units,
                     std::vector<std::string> &lookupNames) {
  ASTBuilder builder(units, lookupNames);
  const int status = tool.run(&builder);
  std::ranges::sort(lookupNames);
  lookupNames.erase(std::ranges::unique(lookupNames).begin(), lookupNames.end());
  return status;
}

} // namespace facts::astcache::detail
