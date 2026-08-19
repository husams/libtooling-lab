#include "ast/FactExtractor.h"

#include "ast/Indexing.h"
#include "ast/visitors/Traversal.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/Tooling.h>

#include <memory>

namespace facts {
namespace {

class FactConsumer final : public clang::ASTConsumer {
public:
  FactConsumer(FileManager &files, FactStore &store, IndexingStatus &status)
      : files_(files), store_(store), status_(status) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    traverse(context, files_, store_, status_);
  }

private:
  FileManager &files_;
  FactStore &store_;
  IndexingStatus &status_;
};

class FactAction final : public clang::ASTFrontendAction {
public:
  FactAction(FileManager &files, FactStore &store, IndexingStatus &status)
      : files_(files), store_(store), status_(status) {}

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &, llvm::StringRef) override {
    return std::make_unique<FactConsumer>(files_, store_, status_);
  }

private:
  FileManager &files_;
  FactStore &store_;
  IndexingStatus &status_;
};

class FactActionFactory final : public clang::tooling::FrontendActionFactory {
public:
  FactActionFactory(FileManager &files, FactStore &store,
                    IndexingStatus &status)
      : files_(files), store_(store), status_(status) {}

  std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<FactAction>(files_, store_, status_);
  }

private:
  FileManager &files_;
  FactStore &store_;
  IndexingStatus &status_;
};

} // namespace

std::unique_ptr<clang::tooling::FrontendActionFactory>
createFactExtractorFactory(FileManager &files, FactStore &store,
                           IndexingStatus &status) {
  return std::make_unique<FactActionFactory>(files, store, status);
}

} // namespace facts
