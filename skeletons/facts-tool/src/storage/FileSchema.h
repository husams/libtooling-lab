#ifndef FACTS_TOOL_STORAGE_FILE_SCHEMA_H
#define FACTS_TOOL_STORAGE_FILE_SCHEMA_H

namespace facts {

inline constexpr const char *fileSchemaSql = R"sql(

DROP TABLE IF EXISTS include_dependency;

CREATE TABLE IF NOT EXISTS semantic_universe (
  id     INTEGER PRIMARY KEY,
  key    TEXT NOT NULL UNIQUE,
  name   TEXT NOT NULL,
  policy TEXT NOT NULL DEFAULT 'explicit'
);

INSERT OR IGNORE INTO semantic_universe(id, key, name, policy)
VALUES(1, 'legacy', 'Legacy single-workspace universe', 'legacy');

CREATE TABLE IF NOT EXISTS repository (
  id                   INTEGER PRIMARY KEY,
  name                 TEXT NOT NULL UNIQUE,
  kind                 TEXT NOT NULL DEFAULT 'repo',
  remote_url           TEXT,
  active_clone_id      INTEGER,
  semantic_universe_id INTEGER NOT NULL DEFAULT 1
    REFERENCES semantic_universe(id) ON DELETE SET DEFAULT
);

CREATE TABLE IF NOT EXISTS clone (
  id            INTEGER PRIMARY KEY,
  repository_id INTEGER NOT NULL REFERENCES repository(id) ON DELETE CASCADE,
  path          TEXT NOT NULL UNIQUE,
  label         TEXT
);

CREATE TABLE IF NOT EXISTS component (
  id                   INTEGER PRIMARY KEY,
  name                 TEXT NOT NULL,
  path                 TEXT NOT NULL,
  kind                 TEXT NOT NULL DEFAULT 'repo',
  version              TEXT,
  repository_id        INTEGER REFERENCES repository(id) ON DELETE SET NULL,
  semantic_universe_id INTEGER
    REFERENCES semantic_universe(id) ON DELETE SET NULL,
  UNIQUE(repository_id, path)
);

CREATE TABLE IF NOT EXISTS directory (
  id           INTEGER PRIMARY KEY,
  component_id INTEGER NOT NULL REFERENCES component(id) ON DELETE CASCADE,
  path         TEXT NOT NULL,
  UNIQUE(component_id, path)
);

CREATE TABLE IF NOT EXISTS file (
  id              INTEGER PRIMARY KEY CHECK(id >= 1),
  directory_id    INTEGER NOT NULL REFERENCES directory(id) ON DELETE CASCADE,
  name            TEXT NOT NULL,
  mtime           REAL,
  md5             TEXT,
  compile_options TEXT,
  driver          TEXT,
  working_directory TEXT,
  indexed         INTEGER NOT NULL DEFAULT 0,
  indexed_at      TEXT,
  args_overridden INTEGER NOT NULL DEFAULT 0,
  UNIQUE(directory_id, name)
);

)sql";

} // namespace facts

#endif // FACTS_TOOL_STORAGE_FILE_SCHEMA_H
