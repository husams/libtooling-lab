#ifndef FACTS_TOOL_STORAGE_FILE_MANAGER_H
#define FACTS_TOOL_STORAGE_FILE_MANAGER_H

#include "model/SymbolId.h"
#include "storage/ProjectConfiguration.h"

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>

namespace facts {

class FileDatabase;

class FileManager {
public:
  // Read-write: creates and migrates the registry; throws when it cannot.
  explicit FileManager(std::string databasePath);

  // Read-only: never throws. An unreadable, incomplete or outdated registry
  // comes back as a message the calling command can report.
  static std::expected<std::unique_ptr<FileManager>, std::string>
  openReadOnly(std::string databasePath);

  ~FileManager();

  FileManager(const FileManager &) = delete;
  FileManager &operator=(const FileManager &) = delete;

  std::expected<void, std::error_code>
  addBulk(std::span<const std::string> paths);
  std::expected<void, std::error_code>
  replaceProjectConfiguration(const ProjectConfiguration &configuration);
  std::expected<void, std::error_code>
  switchActiveClone(std::string_view repositoryName,
                    std::string_view clonePathOrLabel);
  std::expected<void, std::error_code> addClone(std::string_view repositoryName,
                                                const ProjectClone &clone,
                                                bool activate = false);
  std::expected<FileId, std::error_code> getId(std::string_view path);

private:
  FileManager(std::string databasePath, std::unique_ptr<FileDatabase> database);

  std::string databasePath_;
  std::unique_ptr<FileDatabase> database_;
  std::unordered_map<std::string, FileId> fileIds_;
};

} // namespace facts

#endif // FACTS_TOOL_STORAGE_FILE_MANAGER_H
