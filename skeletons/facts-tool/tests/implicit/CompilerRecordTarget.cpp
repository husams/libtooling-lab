#include "ast/extractors/ExternalTarget.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/RelationTarget.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Type.h>
#include <clang/Tooling/Tooling.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace {

void require(bool condition) {
  if (!condition)
    throw std::runtime_error("compiler record target invariant failed");
}

class CopyTypeRecord final : public clang::RecursiveASTVisitor<CopyTypeRecord> {
public:
  bool VisitVarDecl(clang::VarDecl *decl) {
    if (decl->getNameAsString() == "copy")
      record = decl->getType()->getAsCXXRecordDecl();
    return true;
  }

  const clang::CXXRecordDecl *record = nullptr;
};

const clang::CXXRecordDecl *physicalRecord(clang::ASTContext &context) {
  for (auto *decl : context.getTranslationUnitDecl()->decls()) {
    const auto *record = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
    if (record != nullptr && record->getNameAsString() == "Physical")
      return record;
  }
  return nullptr;
}

class RunDirectory {
public:
  explicit RunDirectory(const std::filesystem::path &root) {
    std::filesystem::create_directories(root);
    for (unsigned suffix = 0;; ++suffix) {
      auto candidate = root / ("run-" + std::to_string(suffix));
      std::error_code error;
      if (std::filesystem::create_directory(candidate, error)) {
        path_ = std::move(candidate);
        return;
      }
      if (error)
        throw std::system_error(error);
    }
  }

  ~RunDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

void checkCompilerRecord(const std::filesystem::path &directory) {
  const auto file = directory / "compiler-record.cpp";
  const std::string code = "namespace b022 {\n"
                           "void sibling() {}\n"
                           "void canary() {\n"
                           "  __builtin_va_list list{};\n"
                           "  auto copy = list[0];\n"
                           "  sibling();\n"
                           "  (void)copy;\n"
                           "}\n"
                           "}\n";
  std::ofstream(file) << code;
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      code,
      {"-std=c++23", "-nostdinc", "-nostdinc++", "-target",
       "x86_64-unknown-linux-gnu"},
      file.string());
  require(ast && !ast->getDiagnostics().hasErrorOccurred());

  CopyTypeRecord finder;
  finder.TraverseDecl(ast->getASTContext().getTranslationUnitDecl());
  require(finder.record != nullptr);
  const auto &record = *finder.record->getCanonicalDecl();
  const auto usr = facts::extractUsr(record);
  require(usr && !usr->empty());
  const auto &sourceManager = ast->getASTContext().getSourceManager();
  require(record.isImplicit());
  require(sourceManager.getExpansionLoc(record.getLocation()).isInvalid());
  require(facts::compilerProvided(record, sourceManager));

  facts::FileManager files((directory / "registry.sqlite").string());
  facts::FactStore store(":memory:");
  const auto resolved =
      facts::resolveRelationTarget(record, sourceManager, files, store);
  require(resolved && *resolved);
  const auto id = **resolved;
  require(id.file == facts::builtinFileId);

  const auto loaded = store.load<facts::Symbol>(id);
  require(loaded && loaded->usr == *usr &&
          loaded->qualifiedName == record.getQualifiedNameAsString() &&
          loaded->Kind == clang::index::getSymbolInfo(&record).Kind &&
          (loaded->flags & facts::bit(facts::ImplicitBit)) != 0 &&
          (loaded->flags & facts::bit(facts::ExternalBit)) != 0);
  const auto external = store.isExternal(id);
  require(external && *external && store.count() == 1);

  facts::Symbol caller{};
  caller.usr = "b022::caller";
  caller.qualifiedName = caller.usr;
  const auto callerId = store.save(1, caller);
  require(callerId &&
          store.saveReturnType(*callerId, {id, "__va_list_tag", ""}));

  facts::FactStore missing(":memory:");
  const auto missingCaller = missing.save(1, caller);
  require(missingCaller.has_value());
  const auto missingTarget = facts::SymbolId{id.file, id.index + 1};
  require(!missing.saveReturnType(*missingCaller,
                                  {missingTarget, "__va_list_tag", ""}));

  const auto repeated =
      facts::resolveRelationTarget(record, sourceManager, files, store);
  require(repeated && *repeated && **repeated == id && store.count() == 2);
}

void checkPhysicalRecord(const std::filesystem::path &directory) {
  const auto file = directory / "physical-record.cpp";
  const std::string code = "struct Physical {};\n";
  std::ofstream(file) << code;
  auto ast = clang::tooling::buildASTFromCodeWithArgs(code, {"-std=c++23"},
                                                      file.string());
  require(ast && !ast->getDiagnostics().hasErrorOccurred());
  const auto *record = physicalRecord(ast->getASTContext());
  require(record != nullptr);
  const auto &sourceManager = ast->getASTContext().getSourceManager();
  require(!record->isImplicit());
  require(!facts::compilerProvided(*record, sourceManager));

  facts::FileManager files((directory / "physical-registry.sqlite").string());
  facts::FactStore store(":memory:");
  const auto resolved =
      facts::resolveRelationTarget(*record, sourceManager, files, store);
  require(!resolved &&
          resolved.error() == facts::ExtractionError::RelationTarget &&
          store.count() == 0);

  require(files.addBulk(std::array{file.string()}).has_value());
  const auto registered =
      facts::resolveRelationTarget(*record, sourceManager, files, store);
  require(registered && *registered &&
          (**registered).file != facts::builtinFileId);
}

void check(const std::filesystem::path &root) {
  const RunDirectory directory(root);
  checkCompilerRecord(directory.path());
  checkPhysicalRecord(directory.path());
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
