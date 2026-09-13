import sqlite3
from pathlib import Path


def create_empty_flow_db(path: Path) -> None:
    with sqlite3.connect(path) as db:
        db.executescript(
            """PRAGMA user_version=1;
            CREATE TABLE variable_flow_run(
              run_id INTEGER PRIMARY KEY, created_at TEXT, project_path TEXT,
              facts_path TEXT, function_selector TEXT, variable_selector TEXT,
              sources TEXT, declaration_line INTEGER, max_depth INTEGER,
              engine TEXT, assumptions TEXT, root_function TEXT,
              root_variable TEXT, status TEXT);
            CREATE TABLE variable_flow_node(
              run_id INTEGER, node_id INTEGER, kind TEXT, function_usr TEXT,
              variable_usr TEXT, name TEXT, type TEXT, file TEXT, line INTEGER,
              column_no INTEGER, offset INTEGER, block INTEGER, depth INTEGER);
            CREATE TABLE variable_flow_edge(
              run_id INTEGER, source INTEGER, target INTEGER, kind TEXT,
              callsite INTEGER);
            CREATE TABLE variable_flow_boundary(
              run_id INTEGER, node INTEGER, reason TEXT, detail TEXT, depth INTEGER);
            INSERT INTO variable_flow_run VALUES
              (1, 'now', 'project', 'facts', 'f', 'v', '', NULL, NULL,
               'engine', 'assumptions', 'f', 'v', 'complete');"""
        )
