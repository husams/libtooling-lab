#ifndef FACTS_TOOL_STORAGE_FILE_DATABASE_H
#define FACTS_TOOL_STORAGE_FILE_DATABASE_H

#include "model/SymbolId.h"

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace facts {

class FileDatabase {
public:
  explicit FileDatabase(const std::string &path);
  ~FileDatabase();

  FileDatabase(const FileDatabase &) = delete;
  FileDatabase &operator=(const FileDatabase &) = delete;

  std::expected<void, std::error_code>
  addBulk(std::span<const std::string> identities);
  std::expected<FileId, std::error_code> getId(std::string_view identity);

private:
  struct Connection;
  std::unique_ptr<Connection> connection_;
};

} // namespace facts

#endif // FACTS_TOOL_STORAGE_FILE_DATABASE_H
