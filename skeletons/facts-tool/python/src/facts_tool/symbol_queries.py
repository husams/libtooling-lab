import sqlite3
from collections.abc import Generator, Sequence

from .ids import SymbolId

_SQL = (
    "SELECT s.*,c.canonical_type AS return_type_spelling FROM symbol s "
    "LEFT JOIN callable_return_type c ON c.symbol_id=s.id"
)


def query_rows(
    db: sqlite3.Connection, sql: str, params: Sequence[object] = ()
) -> Generator[sqlite3.Row]:
    cursor = db.execute(sql, params)
    try:
        yield from cursor
    finally:
        cursor.close()


def symbol_rows(
    db: sqlite3.Connection, ids: set[int] | None = None
) -> Generator[sqlite3.Row]:
    if ids is None:
        yield from query_rows(db, _SQL + " ORDER BY s.id")
        return
    ordered = sorted({SymbolId.unpack(value).sqlite for value in ids})
    for start in range(0, len(ordered), 500):
        batch = ordered[start : start + 500]
        placeholders = ",".join("?" for _ in batch)
        yield from query_rows(
            db, _SQL + f" WHERE s.id IN ({placeholders}) ORDER BY s.id", batch
        )
