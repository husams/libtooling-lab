import sqlite3

from .errors import fail

VERSION = 1
TABLES = {
    "variable_flow_run",
    "variable_flow_node",
    "variable_flow_edge",
    "variable_flow_boundary",
}
COLUMNS = {
    "variable_flow_run": {
        "run_id", "created_at", "project_path", "facts_path",
        "function_selector", "variable_selector", "sources",
        "declaration_line", "max_depth", "engine", "assumptions",
        "root_function", "root_variable", "status",
    },
    "variable_flow_node": {
        "run_id", "node_id", "kind", "function_usr", "variable_usr",
        "name", "type", "file", "line", "column_no", "offset", "block",
        "depth",
    },
    "variable_flow_edge": {"run_id", "source", "target", "kind", "callsite"},
    "variable_flow_boundary": {"run_id", "node", "reason", "detail", "depth"},
}


def _columns(db: sqlite3.Connection, table: str) -> set[str]:
    return {str(row[1]) for row in db.execute(f'PRAGMA table_info("{table}")')}


def require_schema(db: sqlite3.Connection) -> None:
    version = int(db.execute("PRAGMA user_version").fetchone()[0])
    if version != VERSION:
        fail("E_SCHEMA", f"unsupported variable-flow schema version {version}")
    names = {
        str(row[0])
        for row in db.execute(
            "SELECT name FROM sqlite_master WHERE type='table'"
        )
    }
    if names != TABLES:
        fail("E_DATABASE_ROLE", "database is not a variable-flow artifact")
    for table, required in COLUMNS.items():
        missing = sorted(required - _columns(db, table))
        if missing:
            fail("E_SCHEMA", f"variable-flow {table} lacks columns: {', '.join(missing)}")
