#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <vector>

struct sqlite3;

namespace facts {

struct StoredCompilationComponent {
  std::int64_t id;
  std::string name;
  std::filesystem::path root;
};

struct StoredCompileFile {
  std::filesystem::path root;
  std::filesystem::path path;
  std::string componentName;
  std::string driver;
  std::string workingDirectory;
  std::string options;
};

using StoredCommandAliases = std::map<std::string, std::string>;

struct StoredCompilationSnapshot {
  std::vector<StoredCompilationComponent> components;
  std::vector<StoredCompileFile> files;
  StoredCommandAliases labels;
};

class StoredDatabase {
public:
  explicit StoredDatabase(sqlite3 *handle = nullptr) noexcept;
  StoredDatabase(const StoredDatabase &) = delete;
  StoredDatabase &operator=(const StoredDatabase &) = delete;
  StoredDatabase(StoredDatabase &&other) noexcept;
  StoredDatabase &operator=(StoredDatabase &&other) noexcept;
  ~StoredDatabase();

  sqlite3 *get() const noexcept;

private:
  sqlite3 *handle_;
};

std::expected<StoredDatabase, std::string>
openStoredDatabase(const std::filesystem::path &path);

std::expected<StoredCompilationSnapshot, std::string>
readStoredCompilation(sqlite3 *database,
                      std::span<const std::string> requestedSources = {});

} // namespace facts
