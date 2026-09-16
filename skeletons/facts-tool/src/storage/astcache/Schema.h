#pragma once

namespace facts::storage::astcache {

inline constexpr const char *schemaSql = R"sql(
CREATE TABLE ast_cache_snapshot (
  key TEXT PRIMARY KEY CHECK(key <> ''),
  source TEXT NOT NULL CHECK(source <> ''),
  working_directory TEXT NOT NULL CHECK(working_directory <> ''),
  generation TEXT NOT NULL CHECK(generation <> ''),
  UNIQUE(key, generation)
) WITHOUT ROWID;

CREATE TABLE ast_cache_input (
  snapshot_key TEXT NOT NULL REFERENCES ast_cache_snapshot(key) ON DELETE CASCADE,
  path TEXT NOT NULL CHECK(path <> ''),
  PRIMARY KEY(snapshot_key, path)
) WITHOUT ROWID;

CREATE TABLE ast_cache_include (
  snapshot_key TEXT NOT NULL REFERENCES ast_cache_snapshot(key) ON DELETE CASCADE,
  source TEXT NOT NULL CHECK(source <> ''),
  target TEXT NOT NULL CHECK(target <> ''),
  PRIMARY KEY(snapshot_key, source, target)
) WITHOUT ROWID;

CREATE TABLE ast_cache_revision (
  snapshot_key TEXT NOT NULL REFERENCES ast_cache_snapshot(key) ON DELETE CASCADE,
  path TEXT NOT NULL CHECK(path <> ''),
  commit_hash TEXT NOT NULL,
  PRIMARY KEY(snapshot_key, path)
) WITHOUT ROWID;

CREATE TABLE ast_cache_artifact (
  snapshot_key TEXT PRIMARY KEY,
  path TEXT NOT NULL CHECK(path <> ''),
  digest TEXT NOT NULL CHECK(digest <> ''),
  generation TEXT NOT NULL CHECK(generation <> ''),
  FOREIGN KEY(snapshot_key, generation)
    REFERENCES ast_cache_snapshot(key, generation) ON DELETE CASCADE
) WITHOUT ROWID;

UPDATE project_registry SET schema_version=2 WHERE id=1;
)sql";

} // namespace facts::storage::astcache
