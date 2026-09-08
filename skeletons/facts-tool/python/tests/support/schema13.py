import hashlib
import sqlite3
from pathlib import Path


def add_schema13(facts: Path, root: Path) -> None:
    with sqlite3.connect(facts) as db:
        db.executescript(
            """
            CREATE TABLE callgraph_run(run_id INTEGER PRIMARY KEY, created_at TEXT,
              project_path TEXT, facts_path TEXT, mode TEXT, path_mode TEXT,
              calls_scope TEXT, components TEXT, max_depth INTEGER, max_nodes INTEGER,
              max_edges INTEGER, time_limit_ms INTEGER, recover_missing INTEGER,
              status TEXT, truncation_reason TEXT, error TEXT);
            CREATE TABLE callgraph_run_root(run_id INTEGER, symbol_id INTEGER,
              usr TEXT);
            CREATE TABLE callgraph_run_target(run_id INTEGER, symbol_id INTEGER,
              usr TEXT);
            CREATE TABLE callgraph_run_edge(run_id INTEGER, source_id INTEGER,
              destination_id INTEGER, kind INTEGER, position INTEGER, file_id INTEGER,
              offset INTEGER, depth INTEGER, cycle INTEGER);
            CREATE TABLE callgraph_run_frontier(run_id INTEGER, symbol_id INTEGER,
              reason TEXT);
            CREATE TABLE callgraph_run_recovery(run_id INTEGER, tu_file_id INTEGER,
              outcome TEXT, diagnostic TEXT);
            CREATE TABLE expression_occurrence(
              occurrence_id INTEGER PRIMARY KEY, identity TEXT UNIQUE, owner_id INTEGER,
              target_id INTEGER, file_id INTEGER, line INTEGER, col INTEGER,
              offset INTEGER, size INTEGER, source_sha256 TEXT, expression_kind TEXT,
              access TEXT, freshness TEXT, unavailable_reason TEXT);
            CREATE TABLE source_region(
              region_id INTEGER PRIMARY KEY, identity TEXT UNIQUE, symbol_id INTEGER,
              file_id INTEGER, line INTEGER, col INTEGER, offset INTEGER, size INTEGER,
              source_sha256 TEXT, symbol_kind TEXT, freshness TEXT,
              unavailable_reason TEXT);
            CREATE TABLE facts_project_provenance(
              file_id INTEGER PRIMARY KEY, path TEXT NOT NULL,
              universe_key TEXT NOT NULL);
            PRAGMA user_version=13;
            """
        )
        source = root / "src" / "main.cpp"
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        owner, field = (1 << 32) | 1, (1 << 32) | 5
        values = (
            owner,
            field,
            1,
            1,
            1,
            4,
            4,
            digest,
            "MemberExpr",
            "write",
            "current",
            None,
        )
        db.execute(
            "INSERT INTO expression_occurrence "
            "VALUES(1,'occ-1',?,?,?,?,?,?,?,?,?,?,?,?)",
            values,
        )
        values = (
            owner,
            field,
            1,
            1,
            1,
            10,
            4,
            digest,
            "MemberExpr",
            "unknown",
            "current",
            "alias",
        )
        db.execute(
            "INSERT INTO expression_occurrence "
            "VALUES(2,'occ-2',?,?,?,?,?,?,?,?,?,?,?,?)",
            values,
        )
        values = (
            owner,
            1,
            1,
            0,
            0,
            len(source.read_bytes()),
            digest,
            "function",
            "current",
            None,
        )
        db.execute(
            "INSERT INTO source_region VALUES(1,'region-1',?,?,?,?,?,?,?,?,?,?)", values
        )
        db.execute(
            "INSERT INTO facts_project_provenance VALUES(1,?,'demo')", (str(source),)
        )
