#include "storage/FileIndexState.h"
#include "storage/FileDatabase.h"
#include "storage/FileManager.h"

#include <sqlite3.h>

#include <cassert>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  assert(argc == 2);
  const std::filesystem::path databasePath = argv[1];
  std::filesystem::remove(databasePath);
  const auto self = std::filesystem::canonical(__FILE__).string();

  // A fresh registry already carries the columns this feature adds; a
  // never-marked file reads back as not indexed, and a mark/read round-trip
  // agrees on every field, including clearing git_commit back to NULL.
  {
    facts::FileManager manager(databasePath.string());
    assert(manager.addBulk(std::vector<std::string>{self}));
    const auto id = manager.getId(self);
    assert(id);

    const auto initial = manager.indexState(self);
    assert(initial && !initial->indexed && initial->indexedAt.empty() &&
           !initial->mtime && initial->factsDb.empty() && !initial->gitCommit);

    const facts::FileIndexRecord record{.id = *id,
                                        .indexedAt = "2026-09-12T10:00:00Z",
                                        .mtime = 12345.5,
                                        .factsDb = "/tmp/facts.sqlite",
                                        .gitCommit = std::string(40, 'a')};
    assert(manager.markIndexed(std::span(&record, 1)));

    const auto marked = manager.indexState(self);
    assert(marked && marked->indexed && marked->indexedAt == record.indexedAt &&
           marked->mtime == record.mtime && marked->factsDb == record.factsDb &&
           marked->gitCommit == record.gitCommit);

    const facts::FileIndexRecord cleared{.id = *id,
                                         .indexedAt = "2026-09-12T11:00:00Z",
                                         .mtime = 9999.0,
                                         .factsDb = "/tmp/facts2.sqlite",
                                         .gitCommit = std::nullopt};
    assert(manager.markIndexed(std::span(&cleared, 1)));
    const auto reread = manager.indexState(self);
    assert(reread && reread->indexed && !reread->gitCommit &&
           reread->factsDb == cleared.factsDb);
  }

  // A registry whose file table predates facts_db/git_commit: reading comes
  // back "not indexed" instead of failing, and a read-write open migrates
  // the columns back in.
  const auto legacyPath = databasePath.string() + "-legacy";
  std::filesystem::remove(legacyPath);
  {
    facts::FileManager manager(legacyPath);
    assert(manager.addBulk(std::vector<std::string>{self}));
  }
  {
    sqlite3 *raw = nullptr;
    assert(sqlite3_open(legacyPath.c_str(), &raw) == SQLITE_OK);
    assert(sqlite3_exec(raw, "ALTER TABLE file DROP COLUMN facts_db", nullptr,
                        nullptr, nullptr) == SQLITE_OK);
    assert(sqlite3_exec(raw, "ALTER TABLE file DROP COLUMN git_commit", nullptr,
                        nullptr, nullptr) == SQLITE_OK);

    const auto present = facts::fileIndexStateColumnsPresent(raw);
    assert(present && !*present);
    const auto state =
        facts::readFileIndexState(raw, facts::firstPhysicalFileId);
    assert(state && !state->indexed && state->indexedAt.empty() &&
           !state->mtime && state->factsDb.empty() && !state->gitCommit);
    sqlite3_close(raw);
  }
  {
    facts::FileManager manager(legacyPath);
    const auto migrated = manager.indexState(self);
    assert(migrated && !migrated->indexed);
  }
  {
    sqlite3 *raw = nullptr;
    assert(sqlite3_open(legacyPath.c_str(), &raw) == SQLITE_OK);
    const auto present = facts::fileIndexStateColumnsPresent(raw);
    assert(present && *present);
    sqlite3_close(raw);
  }
  return 0;
}
