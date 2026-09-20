import sqlite3
from collections.abc import Iterator
from contextlib import closing

from .ids import SymbolId, logical_id
from .paths import FileResolver
from .rows import Row


def _owner_rows(db: sqlite3.Connection, table: str) -> sqlite3.Cursor:
    return db.execute(f'SELECT * FROM "{table}" ORDER BY symbol_id')


def iter_definitions(db: sqlite3.Connection, files: FileResolver) -> Iterator[Row]:
    with closing(_owner_rows(db, "definition")) as sources:
        for source in sources:
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
            yield row


def iter_enumerations(db: sqlite3.Connection) -> Iterator[Row]:
    return _simple(db, "enumeration")


def iter_enumerators(db: sqlite3.Connection) -> Iterator[Row]:
    return _simple(db, "enumerator")


def iter_initializers(db: sqlite3.Connection) -> Iterator[Row]:
    return _simple(db, "variable_initializer", "initializer")


def iter_return_types(db: sqlite3.Connection) -> Iterator[Row]:
    return _simple(db, "callable_return_type", "return_type")


def _simple(
    db: sqlite3.Connection, table: str, view: str | None = None
) -> Iterator[Row]:
    with closing(_owner_rows(db, table)) as sources:
        for source in sources:
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
            yield row
