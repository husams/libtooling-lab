import sqlite3

from .ids import SymbolId, logical_id
from .paths import FileResolver
from .rows import Row


def _owner_rows(db: sqlite3.Connection, table: str) -> list[sqlite3.Row]:
    return list(db.execute(f'SELECT * FROM "{table}" ORDER BY symbol_id'))


def load_definitions(db: sqlite3.Connection, files: FileResolver) -> list[Row]:
    result = []
    for source in _owner_rows(db, "definition"):
        owner = SymbolId.unpack(int(source["symbol_id"]))
        key = logical_id("definition", owner.packed)
        row = dict(source)
        row.update(
            {
                "id": key,
                "symbol_id": owner.packed,
                "file": files.path(int(row["file_id"])),
                "_key": key,
                "_view": "definition",
            }
        )
        result.append(row)
    return result


def load_enumerations(db: sqlite3.Connection) -> list[Row]:
    return _simple(db, "enumeration")


def load_enumerators(db: sqlite3.Connection) -> list[Row]:
    return _simple(db, "enumerator")


def load_initializers(db: sqlite3.Connection) -> list[Row]:
    return _simple(db, "variable_initializer", "initializer")


def load_return_types(db: sqlite3.Connection) -> list[Row]:
    return _simple(db, "callable_return_type", "return_type")


def _simple(db: sqlite3.Connection, table: str, view: str | None = None) -> list[Row]:
    result = []
    for source in _owner_rows(db, table):
        owner = SymbolId.unpack(int(source["symbol_id"]))
        key = logical_id(view or table, owner.packed)
        row = dict(source)
        row.update(
            {
                "id": key,
                "symbol_id": owner.packed,
                "_key": key,
                "_db_id": int(source["symbol_id"]),
                "_view": view or table,
            }
        )
        for field in ("underlying_type",):
            if field in row:
                row[field] = SymbolId.unpack(int(row[field])).packed
        for field in ("is_scoped", "has_fixed_underlying_type"):
            if field in row:
                row[field] = bool(row[field])
        result.append(row)
    return result
