#pragma once

#include "storage/astcache/Database.h"
#include "storage/FileDatabase.h"
#include "storage/catalog/Database.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>

namespace ast_cache_database_test {

namespace fs = std::filesystem;
namespace cache = facts::storage::astcache;

struct Fixture {
  fs::path root;
  fs::path path;

  Fixture() {
    auto pattern = (fs::temp_directory_path() / "facts-cache-db-XXXXXX").string();
    const auto created = mkdtemp(pattern.data());
    assert(created);
    root = fs::canonical(created);
    path = root / "project.db";
    facts::FileDatabase database(path.string());
    facts::ProjectConfiguration configuration{
        .repositoryName = "cache-test",
        .activeClone = {.path = root.string(), .label = "cache-test"},
        .components = {{.name = "cache-test", .path = "."}},
        .files = {{.componentPath = ".", .directory = "src", .name = "source.cpp",
                   .driver = "clang++", .workingDirectory = root.string(),
                   .compileOptions = "[\"-std=c++23\"]"}}};
    assert(database.replaceProjectConfiguration(configuration));
  }

  ~Fixture() { fs::remove_all(root); }

  facts::storage::Database open() const {
    auto database = facts::storage::Database::open(path.string(), SQLITE_OPEN_READWRITE);
    assert(database);
    return std::move(*database);
  }

  void sql(const char *text) const {
    auto database = open();
    assert(database.executeScript(text));
  }

  std::int64_t number(const std::string &sql) const {
    auto database = open();
    auto rows = facts::catalog::query(database, sql,
        [](const facts::storage::Row &row) { return row.integer(0); });
    assert(rows && rows->size() == 1);
    return rows->front();
  }

  facts::astcache::Snapshot snapshot() const {
    const auto source = (root / "src/source.cpp").string();
    const auto header = (root / "include/header.h").string();
    return {.key = "test-command", .source = source, .working_directory = root.string(),
            .generation = "generation-one", .inputs = {{header}, {source}},
            .includes = {{source, header}}, .revisions = {{root.string(), "commit-one"}}};
  }

  facts::astcache::Artifact artifact(const facts::astcache::Snapshot &snapshot) const {
    return {snapshot.key, (root / "cached.ast").string(), "artifact-digest", snapshot.generation};
  }
};

void lifecycle();
void migration();
void failures();

} // namespace ast_cache_database_test
