import sqlite3
from dataclasses import dataclass

from .ids import MASK64, SymbolId
from .symbol_predicate_values import SqlPredicate


@dataclass(frozen=True)
class SymbolWindow:
    predicate: SqlPredicate
    truncated: bool
    cursor: str | None


def _after(after_id: str | int | None) -> SqlPredicate | None:
    if after_id is None:
        return "1", ()
    try:
        boundary = int(after_id)
    except ValueError:
        return None
    if boundary < 0:
        return "1", ()
    if boundary >= MASK64:
        return "0", ()
    if boundary < (1 << 63):
        return "(s.id < 0 OR s.id > ?)", (boundary,)
    return "(s.id < 0 AND s.id > ?)", (SymbolId.unpack(boundary).sqlite,)


def symbol_window(
    db: sqlite3.Connection, after_id: str | int | None, limit: int
) -> SymbolWindow | None:
    """Bound enumeration before filtering, reading at most two IDs into Python."""
    predicate = _after(after_id)
    if predicate is None or limit < 0:
        return None
    where, params = predicate
    sql = f"SELECT s.id FROM symbol s WHERE {where} ORDER BY s.id"
    cursor = db.execute(sql + " LIMIT 2 OFFSET ?", (*params, max(limit - 1, 0)))
    try:
        boundary = cursor.fetchall()
    finally:
        cursor.close()
    if not limit:
        return SymbolWindow(("0", ()), bool(boundary), None)
    if boundary:
        last = int(boundary[0][0])
        bounded = (f"({where}) AND s.id <= ?", (*params, last))
        return SymbolWindow(
            bounded, len(boundary) > 1, str(SymbolId.unpack(last).packed)
        )
    cursor = db.execute(sql + " DESC LIMIT 1", params)
    try:
        tail = cursor.fetchone()
    finally:
        cursor.close()
    marker = None if tail is None else str(SymbolId.unpack(int(tail[0])).packed)
    return SymbolWindow(predicate, False, marker)
