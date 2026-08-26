#include "ast/extractors/File.h"

#include "storage/FileManager.h"

#include <clang/Basic/FileEntry.h>
#include <clang/Basic/SourceManager.h>

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
  const auto path = realPath.empty() ? entry->getName() : realPath;
  return absoluteIdentity(path.str());
}

// The name the translation unit reached the file by, left unresolved. A
// generated source symlinked into the project is registered where the project
// puts it, so that spelling has to survive as far as the registry lookup.
std::optional<std::string> requestedPath(clang::FileEntryRef entry) {
  const std::filesystem::path path(entry.getName().str());
  if (!path.is_absolute()) {
    return std::nullopt;
  }
  return path.lexically_normal().string();
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
    if (auto requested = requestedPath(*entry)) {
      // FileManager falls back to the symlink-resolved identity itself, so a
      // spelling the registry does not know still resolves.
      return files.getId(*requested);
    }
  }
  return extractFilePath(sourceManager, file)
      .and_then([&files](std::string path) { return files.getId(path); });
}

} // namespace facts
