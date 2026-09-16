#include "tooling/astcache/Storage.h"

#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Serialization/PCHContainerOperations.h>
#include <llvm/ADT/ScopeExit.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/LockFileManager.h>
#include <llvm/Support/VirtualFileSystem.h>

#include <system_error>

namespace facts::astcache::detail {

std::unique_ptr<clang::ASTUnit> loadAST(const Entry &entry) {
  // Validate and deserialize under the same nonblocking lock as writers: the
  // AST bytes cannot be replaced between the digest check and Clang's open.
  llvm::LockFileManager lock(entry.ast.string());
  auto acquired = lock.tryLock();
  if (!acquired) {
    llvm::consumeError(acquired.takeError());
    return nullptr;
  }
  if (!*acquired || !validEntry(entry))
    return nullptr;
  // A cache failure must not enter the command's diagnostic consumer: parsing
  // remains the authoritative fallback, including its original diagnostics.
  auto diagnosticOptions = std::make_shared<clang::DiagnosticOptions>();
  auto filesystem = llvm::vfs::createPhysicalFileSystem();
  if (filesystem->setCurrentWorkingDirectory(entry.working_directory.string()))
    return nullptr;
  const auto diagnostics = clang::CompilerInstance::createDiagnostics(
      *filesystem, *diagnosticOptions, new clang::IgnoringDiagConsumer, true);
  clang::FileSystemOptions fileOptions;
  fileOptions.WorkingDir = entry.working_directory.string();
  // ASTReader keeps a reference to its container reader for lazy reads after
  // this function returns, so the owner must outlive every returned ASTUnit.
  static const clang::RawPCHContainerReader reader;
  auto unit = clang::ASTUnit::LoadFromASTFile(
      entry.ast.string(), reader,
      clang::ASTUnit::LoadEverything, std::move(filesystem),
      std::move(diagnosticOptions), diagnostics, fileOptions,
      clang::HeaderSearchOptions{});
  if (!unit || unit->getDiagnostics().hasErrorOccurred())
    return nullptr;
  return unit;
}

std::expected<void, std::string> storeAST(const Entry &entry,
                                        clang::ASTUnit &unit) {
  std::error_code error;
  std::filesystem::create_directories(entry.ast.parent_path(), error);
  if (error)
    return std::unexpected(error.message());
  llvm::LockFileManager lock(entry.ast.string());
  auto acquired = lock.tryLock();
  if (!acquired)
    return std::unexpected(llvm::toString(acquired.takeError()));
  if (!*acquired)
    return std::unexpected("another process is storing this AST");
  llvm::SmallString<256> temporary;
  if (auto created = llvm::sys::fs::createUniqueFile(
          entry.ast.string() + ".%%%%%%.tmp", temporary))
    return std::unexpected(created.message());
  const llvm::scope_exit cleanup([&] { llvm::sys::fs::remove(temporary); });
  if (unit.Save(temporary))
    return std::unexpected("AST serialization failed");
  std::filesystem::rename(temporary.str().str(), entry.ast, error);
  if (error)
    return std::unexpected(error.message());
  // The manifest commits last and hashes the AST too, so readers reject an
  // interrupted write or mismatched concurrent writer rather than load it.
  return writeMetadata(entry, unit);
}

} // namespace facts::astcache::detail
