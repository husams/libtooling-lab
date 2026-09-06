#include "ast/extractors/ExternalTarget.h"
#include "ast/extractors/RelationTarget.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"
#include <clang/Tooling/Tooling.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>

namespace {
class RunDirectory {
public:
  explicit RunDirectory(const std::string &root) {
    llvm::SmallString<256> directory;
    if (auto error =
            llvm::sys::fs::createUniqueDirectory(root + "/run", directory))
      throw std::system_error(error);
    path_ = directory.str().str();
  }

  ~RunDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

void require(bool valid) {
  if (!valid)
    throw std::runtime_error("declaration resolution invariant failed");
}

void check(const std::string &root) {
  std::filesystem::create_directories(root);
  const RunDirectory directory(std::filesystem::absolute(root).string());
  const auto file = (directory.path() / "declarations.cpp").string();
  const std::string code = "int* seed() { return new int; }\n"
                           "void* operator new(decltype(sizeof(0)));\n"
                           "void ordinary();\n";
  std::ofstream(file) << code;
  auto ast =
      clang::tooling::buildASTFromCodeWithArgs(code, {"-std=c++23"}, file);
  require(ast && !ast->getDiagnostics().hasErrorOccurred());
  auto &sm = ast->getASTContext().getSourceManager();
  facts::FileManager files((directory.path() / "registry.sqlite").string());
  facts::FactStore store(":memory:");
  unsigned checked = 0;
  for (auto *decl : ast->getASTContext().getTranslationUnitDecl()->decls()) {
    auto *function = llvm::dyn_cast<clang::FunctionDecl>(decl);
    if (!function || function->getNumParams() > 1 ||
        (function->getNameAsString() != "ordinary" &&
         function->getNameAsString() != "operator new"))
      continue;
    const auto &target =
        facts::visibleTarget(*function->getCanonicalDecl(), sm);
    require(target.getLocation().isValid());
    require(!facts::compilerProvided(target, sm));
    auto missing = facts::resolveRelationTarget(*function, sm, files, store);
    require(!missing &&
            missing.error() == facts::ExtractionError::RelationTarget);
    ++checked;
  }
  require(checked >= 2 && store.count() == 0);
  require(files.addBulk(std::array{file}).has_value());
  for (auto *decl : ast->getASTContext().getTranslationUnitDecl()->decls()) {
    auto *function = llvm::dyn_cast<clang::FunctionDecl>(decl);
    if (!function || function->getNumParams() != 1 ||
        function->getNameAsString() != "operator new")
      continue;
    auto result = facts::resolveRelationTarget(*function, sm, files, store);
    require(result && *result && (**result).file != facts::builtinFileId);
  }
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  try {
    check(argv[1]);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
