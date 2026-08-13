#ifndef FACTS_TOOL_STORAGE_FILE_MANAGER_H
#define FACTS_TOOL_STORAGE_FILE_MANAGER_H

#include "model/SymbolId.h"

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
  explicit FileManager(std::string databasePath);
  ~FileManager();

  FileManager(const FileManager &) = delete;
  FileManager &operator=(const FileManager &) = delete;

  std::expected<void, std::error_code>
  addBulk(std::span<const std::string> paths);
  std::expected<FileId, std::error_code> getId(std::string_view path);

private:
  std::string databasePath_;
  std::unique_ptr<FileDatabase> database_;
  std::unordered_map<std::string, FileId> fileIds_;
};

} // namespace facts

#endif // FACTS_TOOL_STORAGE_FILE_MANAGER_H
