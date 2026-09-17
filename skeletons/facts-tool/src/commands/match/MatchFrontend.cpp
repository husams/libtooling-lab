#include "commands/match/MatchFrontend.h"
#include "tooling/astcache/Cache.h"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace facts::commands::match {
namespace {

class MatchAction final : public clang::ASTFrontendAction {
public:
  MatchAction(clang::ast_matchers::MatchFinder &finder,
              IncludeGraphFacts &includes)
      : finder_(finder), includes_(includes) {}

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &, llvm::StringRef) override {
    return finder_.newASTConsumer();
  }

protected:
  bool BeginSourceFileAction(clang::CompilerInstance &compiler) override {
    if (!clang::ASTFrontendAction::BeginSourceFileAction(compiler))
      return false;
    attachIncludedFiles(compiler, includes_);
    return true;
  }

private:
  clang::ast_matchers::MatchFinder &finder_;
  IncludeGraphFacts &includes_;
};

class MatchActionFactory final : public clang::tooling::FrontendActionFactory {
public:
  MatchActionFactory(clang::ast_matchers::MatchFinder &finder,
                     IncludeGraphFacts &includes)
      : finder_(finder), includes_(includes) {}

  std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<MatchAction>(finder_, includes_);
  }

private:
  clang::ast_matchers::MatchFinder &finder_;
  IncludeGraphFacts &includes_;
};

void announceProgress(std::size_t index, std::size_t total,
                      const std::string &source) {
  if (total > 1)
    llvm::errs() << "[" << index + 1 << "/" << total << "] Processing file "
                 << clang::tooling::getAbsolutePath(source) << ".\n";
}

MatchFrontendResult matchCachedTranslationUnit(
    const clang::tooling::CompilationDatabase &database,
    clang::ast_matchers::MatchFinder &finder, const std::string &source,
    const astcache::Options &astCache) {
  std::vector<std::unique_ptr<clang::ASTUnit>> units;
  MatchFrontendResult result;
  result.status = astcache::buildASTs(database, {source}, units, astCache,
                                    nullptr, false, &result.includes);
  if (result.status != 0 || units.empty()) {
    result.status = result.status == 0 ? 1 : result.status;
    return result;
  }
  for (const auto &unit : units) {
    finder.matchAST(unit->getASTContext());
  }
  return result;
}

} // namespace

MatchFrontendResult runTranslationUnit(
    const clang::tooling::CompilationDatabase &database,
    clang::ast_matchers::MatchFinder &finder, const std::string &source,
    std::size_t index, std::size_t total, const astcache::Options &astCache) {
  announceProgress(index, total, source);
  if (astCache.enabled)
    return matchCachedTranslationUnit(database, finder, source, astCache);
  IncludeGraphFacts includes;
  clang::tooling::ClangTool tool(database, std::vector<std::string>{source});
  auto factory = std::make_unique<MatchActionFactory>(finder, includes);
  return {.status = tool.run(factory.get()), .includes = std::move(includes)};
}

} // namespace facts::commands::match
