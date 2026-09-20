import sqlite3
from collections.abc import Generator, Sequence
from contextlib import closing

from .catalog_kinds import NODE_KINDS, symbol_kind
from .ids import SymbolId, logical_id
from .paths import FileResolver
from .rows import Row, short_name
from .symbol_queries import _SQL, query_rows, symbol_rows


def _symbol(row: sqlite3.Row, files: FileResolver) -> Row:
    identity = SymbolId.unpack(int(row["id"]))
    qualified = str(row["qualified_name"])
    value = dict(row)
    value.update(
        {
            "id": identity.packed,
            "identity": identity.to_dict(),
            "file_id": identity.file_id,
            "file": files.path(identity.file_id),
            "name": short_name(qualified),
            "spelling": short_name(qualified),
            "kind_id": int(row["kind"]),
            "kind": symbol_kind(int(row["kind"])),
            "node_kind": NODE_KINDS.get(int(row["node"]), "symbol"),
            "_db_id": int(row["id"]),
            "_key": logical_id("symbol", identity.packed),
            "_view": "symbol",
        }
    )
    for key, item in tuple(value.items()):
        if key.startswith("is_") or key.startswith("has_"):
            value[key] = bool(item)
    return value


def iter_symbol_query(
    db: sqlite3.Connection, files: FileResolver, where: str, params: Sequence[object]
) -> Generator[Row]:
    sql = _SQL + (" WHERE " + where if where else "") + " ORDER BY s.id"
    with closing(query_rows(db, sql, params)) as rows:
        yield from (_symbol(row, files) for row in rows)


def iter_symbols(
    db: sqlite3.Connection, files: FileResolver, ids: set[int] | None = None
) -> Generator[Row]:
    with closing(symbol_rows(db, ids)) as rows:
        yield from (_symbol(row, files) for row in rows)


def load_symbols(
    db: sqlite3.Connection, files: FileResolver, ids: set[int] | None = None
) -> list[Row]:
    return list(iter_symbols(db, files, ids))


def lookup_symbol(db: sqlite3.Connection, files: FileResolver, ref: str) -> list[Row]:
    for field in ("usr", "qualified_name"):
        matches = [
            _symbol(row, files)
            for row in query_rows(
                db, _SQL + f" WHERE s.{field}=? ORDER BY s.id", (ref,)
            )
        ]
        if matches:
            return matches
    ids = {
        int(row["id"])
        for row in query_rows(db, "SELECT id,qualified_name FROM symbol")
        if short_name(str(row["qualified_name"])) == ref
    }
    return load_symbols(db, files, ids)
