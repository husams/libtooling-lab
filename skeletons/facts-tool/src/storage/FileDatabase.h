#ifndef FACTS_TOOL_STORAGE_FILE_DATABASE_H
#define FACTS_TOOL_STORAGE_FILE_DATABASE_H

#include "storage/ProjectConfiguration.h"
#include "storage/SqliteDatabase.h"

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
  // Read-write: creates the registry and migrates an outdated one in place.
  // Throws when that is impossible, as the rest of the storage layer does.
  explicit FileDatabase(const std::string &path);

  // Read-only: never throws. An unreadable or outdated registry comes back as
  // a message the calling command can report.
  static std::expected<std::unique_ptr<FileDatabase>, std::string>
  openReadOnly(const std::string &path);

  // Extraction additionally requires at least one stored compile command.
  static std::expected<std::unique_ptr<FileDatabase>, std::string>
  openImportedReadOnly(const std::string &path);

  ~FileDatabase();

  FileDatabase(const FileDatabase &) = delete;
  FileDatabase &operator=(const FileDatabase &) = delete;

  // How many files the registry holds right now.
  std::expected<std::size_t, std::error_code> fileCount();

  // Returns how many file rows the call added; a repeated import adds none.
  std::expected<std::size_t, std::error_code>
  addBulk(std::span<const std::string> identities);
  // Refuses with the name of the offending repository, clone, component or
  // file field; storage failures come back as their own message.
  std::expected<void, std::string>
  replaceProjectConfiguration(const ProjectConfiguration &configuration);
  std::expected<void, std::error_code>
  switchActiveClone(std::string_view repositoryName,
                    std::string_view clonePathOrLabel);
  std::expected<void, std::error_code> addClone(std::string_view repositoryName,
                                                const ProjectClone &clone,
                                                bool activate);
  std::expected<FileId, std::error_code> getId(std::string_view identity);

private:
  explicit FileDatabase(storage::Database database);

  std::expected<void, std::error_code>
  storeProjectConfiguration(const ProjectConfiguration &configuration);

  storage::Database database_;
};

} // namespace facts

#endif // FACTS_TOOL_STORAGE_FILE_DATABASE_H
