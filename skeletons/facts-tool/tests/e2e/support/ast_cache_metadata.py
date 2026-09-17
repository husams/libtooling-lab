"""Project-database observations for native dependency-cache acceptance tests."""
from __future__ import annotations

import hashlib
import sqlite3
import subprocess
from dataclasses import dataclass, field
from pathlib import Path

from support.database import file_snapshot, query

TABLE_COLUMNS = {
    "ast_cache_snapshot": "key,source,working_directory,generation",
    "ast_cache_input": "snapshot_key,path",
    "ast_cache_include": "snapshot_key,source,target",
    "ast_cache_revision": "snapshot_key,path,commit_hash",
    "ast_cache_artifact": "snapshot_key,path,digest,generation",
}


def snapshot(project):
    return {table: sorted(query(project.conf, f"SELECT {columns} FROM {table}"), key=repr)
            for table, columns in TABLE_COLUMNS.items()}


def registry_records(project):
    return {
        "files": file_snapshot(project.conf),
        "commands": query(project.conf, "SELECT id,compile_options,driver,"
                          "working_directory,args_overridden FROM file ORDER BY id"),
        "matched": query(project.conf, "SELECT * FROM matched_symbol_index ORDER BY usr,file_id"),
    }


def git_head(project):
    result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=project.root,
                            env=project.environment, text=True, capture_output=True,
                            check=True, timeout=10)
    return result.stdout.strip()


def require_dependency_hit(project):
    project.succeed()
    diagnostics = project.last.stderr
    assert "dependency-cache: hit" in diagnostics, diagnostics
    assert "dependency-cache: miss" not in diagnostics, diagnostics
    assert "dependency-cache: stored" not in diagnostics, diagnostics
    if project.last_family == "import":
        assert "ast-cache: miss" not in diagnostics, diagnostics
        assert "ast-cache: stored" not in diagnostics, diagnostics
    else:
        assert "ast-cache:" not in diagnostics, diagnostics


def require_import_stored(project):
    project.succeed()
    assert project.last.stderr.count("ast-cache: miss") == 1, project.last.stderr
    assert project.last.stderr.count("ast-cache: stored") == 1, project.last.stderr
    assert "dependency-cache: miss" not in project.last.stderr, project.last.stderr
    assert "dependency-cache: stored" not in project.last.stderr, project.last.stderr


def require_normalized_inputs(project):
    rows = snapshot(project)
    assert len(rows["ast_cache_snapshot"]) == 1, rows
    key, source, working_directory, generation = rows["ast_cache_snapshot"][0]
    assert source == str(project.source)
    assert working_directory == str(project.root)
    assert generation
    assert {(key, str(project.source)), (key, str(project.header))} <= set(rows["ast_cache_input"])
    assert (key, str(project.source), str(project.header)) in rows["ast_cache_include"]
    assert (key, str(project.root), git_head(project)) in rows["ast_cache_revision"]
    for _key, path in rows["ast_cache_input"]:
        assert _key == key
        assert Path(path).is_absolute(), path
    for table in ("ast_cache_input", "ast_cache_include", "ast_cache_revision"):
        assert len(rows[table]) == len(set(rows[table])), rows[table]
    assert not query(project.conf, "PRAGMA foreign_key_check")


def require_imported_artifact(project):
    project.succeed()
    rows = snapshot(project)
    assert len(rows["ast_cache_artifact"]) == 1, rows
    key, path, digest, generation = rows["ast_cache_artifact"][0]
    artifact = Path(path)
    assert artifact in project.ast_files(), path
    assert artifact.stat().st_size > 100
    assert hashlib.sha256(artifact.read_bytes()).hexdigest() == digest
    assert (key, str(project.source), str(project.root), generation) in rows["ast_cache_snapshot"]
    assert not tuple(project.cache.rglob("*.json"))


def require_include_fact(project):
    source = query(project.conf, "SELECT id FROM file WHERE name='cache.cpp'")
    header = query(project.conf, "SELECT id FROM file WHERE name='cache.hpp'")
    assert len(source) == len(header) == 1
    assert query(project.facts, "SELECT src_file_id,dst_file_id FROM include_dependency "
                 "WHERE src_file_id=? AND dst_file_id=?", (source[0][0], header[0][0]))


def downgrade_project(project, version):
    with sqlite3.connect(project.conf) as connection:
        for table in reversed(TABLE_COLUMNS):
            connection.execute(f"DROP TABLE {table}")
        if version == 1:
            connection.execute("UPDATE project_registry SET schema_version=1 WHERE id=1")
            return
        connection.execute("DROP TABLE matched_symbol_index")
        connection.executescript("""
            CREATE TABLE legacy_registry (
                id INTEGER PRIMARY KEY CHECK(id=1), complete INTEGER NOT NULL DEFAULT 0,
                fingerprint TEXT NOT NULL DEFAULT '', file_count INTEGER NOT NULL DEFAULT 0);
            INSERT INTO legacy_registry SELECT id,complete,fingerprint,file_count FROM project_registry;
            DROP TABLE project_registry;
            ALTER TABLE legacy_registry RENAME TO project_registry;
        """)


@dataclass
class MetadataObservation:
    rows: dict = field(default_factory=dict)
    registry: dict = field(default_factory=dict)
    artifacts: dict = field(default_factory=dict)
    sidecars: dict = field(default_factory=dict)
