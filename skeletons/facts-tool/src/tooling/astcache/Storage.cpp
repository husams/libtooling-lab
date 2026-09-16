#include "tooling/astcache/Storage.h"

#include "tooling/astcache/Serialization.h"

#include <clang/Frontend/ASTUnit.h>
#include <llvm/ADT/ScopeExit.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/LockFileManager.h>

#include <system_error>

namespace facts::astcache::detail {

std::unique_ptr<clang::ASTUnit> loadAST(const Entry &entry) {
  // Validate and deserialize under the same nonblocking lock as writers.
  llvm::LockFileManager lock(entry.ast.string());
  auto acquired = lock.tryLock();
  if (!acquired) {
    llvm::consumeError(acquired.takeError());
    return nullptr;
  }
  if (!*acquired || !validEntry(entry))
    return nullptr;
  return loadSerialized(entry.ast, entry.working_directory);
}

std::expected<void, std::string> storeAST(const Entry &entry,
                                        clang::ASTUnit &unit,
                                        const Snapshot &snapshot) {
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
#if LLVM_VERSION_MAJOR >= 22
  const llvm::scope_exit cleanup([&] { llvm::sys::fs::remove(temporary); });
#else
  const auto cleanup =
      llvm::make_scope_exit([&] { llvm::sys::fs::remove(temporary); });
#endif
  if (auto saved = saveSerialized(unit, temporary.str().str()); !saved)
    return std::unexpected(saved.error());
  std::filesystem::rename(temporary.str().str(), entry.ast, error);
  if (error)
    return std::unexpected(error.message());
  // Publish snapshot and artifact metadata together only after the AST is
  // complete. Generation and digest reject an interrupted replacement.
  return writeMetadata(entry, snapshot);
}

} // namespace facts::astcache::detail
