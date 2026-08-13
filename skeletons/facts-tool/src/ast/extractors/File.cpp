#include "ast/extractors/File.h"

#include "storage/FileManager.h"

#include <clang/Basic/FileEntry.h>
#include <clang/Basic/SourceManager.h>

#include <filesystem>

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

std::expected<FileId, std::error_code>
resolveFile(const clang::SourceManager &sourceManager,
            clang::SourceLocation location, FileManager &files) {
  const auto expansion = sourceManager.getExpansionLoc(location);
  if (expansion.isInvalid()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  return extractFilePath(sourceManager, sourceManager.getFileID(expansion))
      .and_then([&files](std::string path) { return files.getId(path); });
}

} // namespace facts
