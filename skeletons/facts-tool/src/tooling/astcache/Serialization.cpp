#include "tooling/astcache/Serialization.h"

#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Serialization/PCHContainerOperations.h>
#include <llvm/ADT/ScopeExit.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SaveAndRestore.h>
#include <llvm/Support/VirtualFileSystem.h>

#include <utility>
#include <vector>

namespace facts::astcache::detail {
namespace {

using SourceBuffers = std::vector<clang::SrcMgr::ContentCache *>;

std::expected<void, std::string>
closeFinalExpansion(clang::SourceManager &manager) {
  if (!manager.getLocalSLocEntry(manager.local_sloc_entry_size() - 1)
           .isExpansion())
    return {};
  // LLVM 21/22 give the final loaded source-location entry one extra byte.
  // For a macro, that breaks Lexer's end-of-expansion check and can change
  // original initializer spelling. An empty buffer provides an explicit next
  // entry boundary without changing any existing source location or token.
  const auto boundary = manager.createFileID(
      llvm::MemoryBuffer::getMemBufferCopy("", "<facts-ast-cache-end>"),
      clang::SrcMgr::C_User);
  if (boundary.isInvalid())
    return std::unexpected("AST source-location boundary is unavailable");
  return {};
}

std::expected<SourceBuffers, std::string>
parsedSourceBuffers(clang::SourceManager &manager) {
  // Never reread a source file while serializing: its contents could already
  // differ from the bytes that produced the AST and its source offsets.
  for (unsigned index = 1; index < manager.local_sloc_entry_size(); ++index) {
    const auto &entry = manager.getLocalSLocEntry(index);
    if (!entry.isFile())
      continue;
    const auto &cache = entry.getFile().getContentCache();
    if (cache.OrigEntry &&
        (!cache.getBufferIfLoaded() || cache.IsBufferInvalid))
      return std::unexpected("parsed source buffer is unavailable: " +
                             cache.OrigEntry->getName().str());
  }

  SourceBuffers buffers;
  for (auto info = manager.fileinfo_begin(); info != manager.fileinfo_end();
       ++info) {
    if (info->second->getBufferIfLoaded() && !info->second->BufferOverridden)
      buffers.push_back(info->second);
  }
  return buffers;
}

std::expected<void, std::string>
writeEmbedded(clang::ASTUnit &unit, const std::filesystem::path &path,
              const SourceBuffers &buffers) {
  // The parsed AST already contains the effects of the sysroot and overlays.
  // Do not serialize instructions to reread mutable overlay files, or add the
  // SDKSettings.json validation input which ASTWriter normally creates for a
  // sysroot. HeaderSearch exposes a const view of its mutable invocation-owned
  // options; these two serialization-only changes are restored before return.
  auto &options = const_cast<clang::HeaderSearchOptions &>(
      unit.getPreprocessor().getHeaderSearchInfo().getHeaderSearchOpts());
  const llvm::SaveAndRestore restoreSysroot(options.Sysroot, std::string{});
  const llvm::SaveAndRestore restoreOverlays(options.VFSOverlayFiles,
                                            std::vector<std::string>{});

  // ASTWriter embeds buffers marked as overridden; ASTReader restores these
  // bytes and accepts an edited or missing physical input. Replacing buffers
  // through overrideFileContents here would free memory still referenced by
  // preprocessor literal tokens. Mark the public serialization flag instead,
  // preserving every buffer address, and restore it after serialization.
  for (auto *buffer : buffers)
    buffer->BufferOverridden = true;
#if LLVM_VERSION_MAJOR >= 22
  const llvm::scope_exit restore([&] {
#else
  const auto restore = llvm::make_scope_exit([&] {
#endif
    for (auto *buffer : buffers)
      buffer->BufferOverridden = false;
  });
  if (unit.Save(path.string()))
    return std::unexpected("AST serialization failed");
  return {};
}

} // namespace

std::expected<void, std::string>
saveSerialized(clang::ASTUnit &unit, const std::filesystem::path &path) {
  if (!unit.hasSema() || unit.getDiagnostics().hasErrorOccurred())
    return std::unexpected("AST is unavailable for serialization");
  return parsedSourceBuffers(unit.getSourceManager())
      .and_then([&](const SourceBuffers &buffers) {
        return closeFinalExpansion(unit.getSourceManager()).and_then([&] {
          return writeEmbedded(unit, path, buffers);
        });
      });
}

std::unique_ptr<clang::ASTUnit>
loadSerialized(const std::filesystem::path &path,
               const std::filesystem::path &workingDirectory) {
  auto diagnosticOptions = std::make_shared<clang::DiagnosticOptions>();
  auto filesystem = llvm::vfs::createPhysicalFileSystem();
  if (filesystem->setCurrentWorkingDirectory(workingDirectory.string()))
    return nullptr;
  const auto diagnostics = clang::CompilerInstance::createDiagnostics(
      *filesystem, *diagnosticOptions, new clang::IgnoringDiagConsumer, true);
  clang::FileSystemOptions fileOptions;
  fileOptions.WorkingDir = workingDirectory.string();
  // Lazy deserialization retains this reader beyond the function's lifetime.
  static const clang::RawPCHContainerReader reader;
#if LLVM_VERSION_MAJOR >= 22
  auto unit = clang::ASTUnit::LoadFromASTFile(
      path.string(), reader, clang::ASTUnit::LoadEverything,
      std::move(filesystem), std::move(diagnosticOptions), diagnostics,
      fileOptions, clang::HeaderSearchOptions{});
#else
  auto unit = clang::ASTUnit::LoadFromASTFile(
      path.string(), reader, clang::ASTUnit::LoadEverything,
      std::move(diagnosticOptions), diagnostics, fileOptions,
      clang::HeaderSearchOptions{}, nullptr, false,
      clang::CaptureDiagsKind::None, false, false, std::move(filesystem));
#endif
  if (!unit || unit->getDiagnostics().hasErrorOccurred())
    return nullptr;
  return unit;
}

} // namespace facts::astcache::detail
