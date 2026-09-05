import sqlite3

from .catalog_kinds import NODE_KINDS, symbol_kind
from .ids import SymbolId, logical_id
from .paths import FileResolver
from .rows import Row, short_name

_SQL = (
    "SELECT s.*,c.canonical_type AS return_type_spelling FROM symbol s "
    "LEFT JOIN callable_return_type c ON c.symbol_id=s.id ORDER BY s.id"
)


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


def load_symbols(
    db: sqlite3.Connection, files: FileResolver, ids: set[int] | None = None
) -> list[Row]:
    rows = [_symbol(row, files) for row in db.execute(_SQL)]
    if ids is None:
        return rows
    return [row for row in rows if int(row["id"]) in ids]


def lookup_symbol(db: sqlite3.Connection, files: FileResolver, ref: str) -> list[Row]:
    all_rows = load_symbols(db, files)
    for field in ("usr", "qualified_name", "spelling"):
        matches = [row for row in all_rows if row[field] == ref]
        if matches:
            return matches
    return []
