#include "ast/extractors/File.h"

#include "storage/FileManager.h"

#include <clang/Basic/FileEntry.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/SmallString.h>

#include <filesystem>
#include <optional>

namespace facts {
namespace {

std::expected<std::string, std::error_code>
absoluteIdentity(std::string_view path) {
  std::error_code error;
  auto identity = std::filesystem::canonical(path, error);
  if (error) {
    return std::unexpected(error);
  }
  if (!identity.is_absolute()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  return identity.lexically_normal().string();
}

} // namespace

std::expected<std::string, std::error_code>
extractFilePath(const clang::SourceManager &sourceManager, clang::FileID file) {
  auto entry = sourceManager.getFileEntryRefForID(file);
  if (!entry) {
    return std::unexpected(
        std::make_error_code(std::errc::no_such_file_or_directory));
  }

  const auto realPath = entry->getFileEntry().tryGetRealPathName();
  llvm::SmallString<256> path(realPath.empty() ? entry->getName() : realPath);
  sourceManager.getFileManager().makeAbsolutePath(path);
  return absoluteIdentity(path.str().str());
}

// The name the translation unit reached the file by, left unresolved. A
// generated source symlinked into the project is registered where the project
// puts it, so that spelling has to survive as far as the registry lookup.
std::optional<std::string> requestedPath(const clang::SourceManager &manager,
                                        clang::FileEntryRef entry) {
  llvm::SmallString<256> spelling(entry.getName());
  manager.getFileManager().makeAbsolutePath(spelling);
  const std::filesystem::path path(spelling.str().str());
  if (!path.is_absolute()) {
    return std::nullopt;
  }
  // FileManager canonicalizes the physical path before trying its normalized
  // registry alias. Collapsing '..' here would lose preceding symlinks, such
  // as /lib -> /usr/lib in GCC's standard-library search paths.
  return path.string();
}

std::expected<FileId, std::error_code>
resolveFile(const clang::SourceManager &sourceManager,
            clang::SourceLocation location, FileManager &files) {
  const auto expansion = sourceManager.getExpansionLoc(location);
  if (expansion.isInvalid()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  const auto file = sourceManager.getFileID(expansion);
  if (auto entry = sourceManager.getFileEntryRefForID(file)) {
    if (auto requested = requestedPath(sourceManager, *entry)) {
      // FileManager falls back to the symlink-resolved identity itself, so a
      // spelling the registry does not know still resolves.
      return files.getId(*requested);
    }
  }
  return extractFilePath(sourceManager, file)
      .and_then([&files](std::string path) { return files.getId(path); });
}

} // namespace facts
