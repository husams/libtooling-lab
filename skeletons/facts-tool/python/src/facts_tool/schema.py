import sqlite3
from dataclasses import dataclass

from .errors import fail
from .schema_catalog import COLUMNS, FACTS_TABLES, PROJECT_TABLES


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
    if role == "facts" and facts_shape and version != 10:
        fail("E_SCHEMA", f"facts schema user_version {version} is unsupported; need 10")
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
