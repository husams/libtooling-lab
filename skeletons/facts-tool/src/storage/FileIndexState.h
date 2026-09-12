#ifndef FACTS_TOOL_STORAGE_FILE_INDEX_STATE_H
#define FACTS_TOOL_STORAGE_FILE_INDEX_STATE_H

#include "model/SymbolId.h"

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <system_error>

struct sqlite3;

namespace facts {

namespace storage {
class Database;
}

// What the file row says right now.
struct FileIndexState {
  bool indexed = false;
  std::string indexedAt; // '' when NULL
  std::optional<double> mtime;
  std::string factsDb; // '' when NULL
  std::optional<std::string> gitCommit;
};

// What extraction writes back for one file.
struct FileIndexRecord {
  FileId id;
  std::string indexedAt;
  double mtime;
  std::string factsDb;
  std::optional<std::string> gitCommit;
};

// Whether the file table carries the facts_db/git_commit columns this
// feature adds. A registry a writer has not migrated yet -- an old database
// opened read-only -- answers false instead of an error.
std::expected<bool, std::error_code>
fileIndexStateColumnsPresent(sqlite3 *database);

// Reads the row directly, without checking whether the columns exist --
// callers that already know they do (see fileIndexStateColumnsPresent, and
// FileDatabase::indexState's own cached check) skip re-running that check
// per file this way. Fails with a SQL error against a database that does
// not carry the columns.
std::expected<FileIndexState, std::error_code>
readFileIndexStateRow(sqlite3 *database, FileId id);

// readFileIndexStateRow(), after confirming the columns are present;
// answers "not indexed" (every field at its default) rather than failing
// when they are absent: a read-only handle on an unmigrated registry has
// nothing to say about index state yet.
std::expected<FileIndexState, std::error_code>
readFileIndexState(sqlite3 *database, FileId id);

// Marks every record's file row indexed with the state extraction just
// produced, in a single transaction.
std::expected<void, std::error_code>
markFilesIndexed(storage::Database &database,
                 std::span<const FileIndexRecord> records);

} // namespace facts

#endif // FACTS_TOOL_STORAGE_FILE_INDEX_STATE_H
