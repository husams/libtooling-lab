import sqlite3
from dataclasses import dataclass

from .errors import fail
from .schema_catalog import COLUMNS, EVIDENCE_TABLES, FACTS_TABLES, PROJECT_TABLES
from .schema_graph import CALLGRAPH_COLUMNS, CALLGRAPH_TABLES


@dataclass(frozen=True)
class SchemaIdentity:
    role: str
    user_version: int
    schema_version: int
    tables: tuple[str, ...]

    def to_dict(self) -> dict[str, object]:
        return {
            "role": self.role,
            "user_version": self.user_version,
            "schema_version": self.schema_version,
            "tables": list(self.tables),
        }


def _scalar(db: sqlite3.Connection, pragma: str) -> int:
    row = db.execute(pragma).fetchone()
    return int(row[0]) if row else 0


def inspect_schema(db: sqlite3.Connection, role: str) -> SchemaIdentity:
    rows = db.execute(
        "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"
    ).fetchall()
    tables = tuple(str(row[0]) for row in rows)
    required = FACTS_TABLES if role == "facts" else PROJECT_TABLES
    version = _scalar(db, "PRAGMA user_version")
    facts_shape = {"symbol", "relation"} <= set(tables)
    if role == "facts" and facts_shape and version not in (10, 11, 12, 13):
        fail(
            "E_SCHEMA",
            "facts schema user_version "
            f"{version} is unsupported; need 10, 11, 12, or 13",
        )
    if role == "facts" and version in (12, 13):
        missing_graph = sorted(CALLGRAPH_TABLES - set(tables))
        if missing_graph:
            fail(
                "E_SCHEMA",
                "unsupported schema 12 layout; lacks callgraph tables: "
                + ", ".join(missing_graph),
            )
        for table in CALLGRAPH_TABLES:
            actual = {
                str(row[1]) for row in db.execute(f'PRAGMA table_info("{table}")')
            }
            absent = sorted(CALLGRAPH_COLUMNS[table] - actual)
            if absent:
                fail("E_SCHEMA", f"facts.{table} lacks columns: " + ", ".join(absent))
    if role == "facts" and version == 13:
        missing_evidence = sorted(EVIDENCE_TABLES - set(tables))
        if missing_evidence:
            fail(
                "E_SCHEMA",
                "unsupported schema 13 layout; lacks evidence tables: "
                + ", ".join(missing_evidence),
            )
        for table in EVIDENCE_TABLES:
            actual = {
                str(row[1]) for row in db.execute(f'PRAGMA table_info("{table}")')
            }
            absent = sorted(COLUMNS[table] - actual)
            if absent:
                fail("E_SCHEMA", f"facts.{table} lacks columns: " + ", ".join(absent))
    missing = sorted(required - set(tables))
    if missing:
        fail("E_DATABASE_ROLE", f"{role} database lacks tables: {', '.join(missing)}")
    for table in required & COLUMNS.keys():
        actual = {str(row[1]) for row in db.execute(f'PRAGMA table_info("{table}")')}
        absent = sorted(COLUMNS[table] - actual)
        if absent:
            fail("E_SCHEMA", f"{role}.{table} lacks columns: {', '.join(absent)}")
    return SchemaIdentity(role, version, _scalar(db, "PRAGMA schema_version"), tables)


def has_table(schema: SchemaIdentity, table: str) -> bool:
    return table in schema.tables
