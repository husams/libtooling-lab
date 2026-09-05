CREATE TABLE semantic_universe (
 id INTEGER PRIMARY KEY, key TEXT, name TEXT, policy TEXT
);
CREATE TABLE repository (
 id INTEGER PRIMARY KEY, name TEXT, kind TEXT, remote_url TEXT,
 active_clone_id INTEGER, semantic_universe_id INTEGER
);
CREATE TABLE clone (
 id INTEGER PRIMARY KEY, repository_id INTEGER, path TEXT, label TEXT
);
CREATE TABLE component (
 id INTEGER PRIMARY KEY, name TEXT, path TEXT, kind TEXT, version TEXT,
 repository_id INTEGER, semantic_universe_id INTEGER
);
CREATE TABLE directory (
 id INTEGER PRIMARY KEY, component_id INTEGER, path TEXT
);
CREATE TABLE file (
 id INTEGER PRIMARY KEY, directory_id INTEGER, name TEXT, mtime REAL, md5 TEXT,
 compile_options TEXT, driver TEXT, working_directory TEXT, indexed INTEGER,
 indexed_at TEXT, args_overridden INTEGER
);
CREATE TABLE project_registry (
 id INTEGER PRIMARY KEY, complete INTEGER, fingerprint TEXT, file_count INTEGER
);
