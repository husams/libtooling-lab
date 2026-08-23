#include "ast/visitors/IncludeVisitor.h"

#include "ast/extractors/File.h"

#include <clang/Basic/FileEntry.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Tooling/Tooling.h>

#include <filesystem>
#include <optional>
#include <utility>

namespace facts {
namespace {

std::string pathOf(clang::FileEntryRef file) {
  const auto realPath = file.getFileEntry().tryGetRealPathName();
  const auto path = realPath.empty() ? file.getName() : realPath;
  return std::filesystem::absolute(path.str()).lexically_normal().string();
}

class IncludeVisitor final : public clang::PPCallbacks {
public:
  IncludeVisitor(clang::SourceManager &sourceManager, IncludeGraphFacts &facts)
      : sourceManager_(sourceManager), facts_(facts) {}

  void InclusionDirective(clang::SourceLocation hashLocation,
                          const clang::Token &, llvm::StringRef, bool,
                          clang::CharSourceRange,
                          clang::OptionalFileEntryRef file, llvm::StringRef,
                          llvm::StringRef, const clang::Module *, bool,
                          clang::SrcMgr::CharacteristicKind) override {
    if (!file) {
      return;
    }
    auto source = extractFilePath(
        sourceManager_,
        sourceManager_.getFileID(sourceManager_.getExpansionLoc(hashLocation)));
    if (!source) {
      return;
    }
    auto destination = pathOf(*file);
    facts_.visitedSources.push_back(*source);
    facts_.visitedSources.push_back(destination);
    facts_.edges.push_back({std::move(*source), std::move(destination)});
  }

private:
  clang::SourceManager &sourceManager_;
  IncludeGraphFacts &facts_;
};

class IncludeAction final : public clang::PreprocessOnlyAction {
public:
  explicit IncludeAction(IncludeGraphFacts &facts) : facts_(facts) {}

protected:
  bool BeginSourceFileAction(clang::CompilerInstance &compiler) override {
    auto &sourceManager = compiler.getSourceManager();
    auto main = extractFilePath(sourceManager, sourceManager.getMainFileID());
    if (main) {
      facts_.visitedSources.push_back(std::move(*main));
    }
    compiler.getPreprocessor().addPPCallbacks(
        std::make_unique<IncludeVisitor>(sourceManager, facts_));
    return true;
  }

private:
  IncludeGraphFacts &facts_;
};

class IncludeActionFactory final
    : public clang::tooling::FrontendActionFactory {
public:
  explicit IncludeActionFactory(IncludeGraphFacts &facts) : facts_(facts) {}

  std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<IncludeAction>(facts_);
  }

private:
  IncludeGraphFacts &facts_;
};

} // namespace

std::unique_ptr<clang::tooling::FrontendActionFactory>
createIncludeVisitorFactory(IncludeGraphFacts &facts) {
  return std::make_unique<IncludeActionFactory>(facts);
}

} // namespace facts
