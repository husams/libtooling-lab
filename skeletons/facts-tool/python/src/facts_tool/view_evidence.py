import sqlite3
from typing import cast

from .ids import SymbolId, logical_id
from .paths import FileResolver
from .rows import Row


def _symbol_id(value: object) -> int:
    return SymbolId.unpack(int(cast(int, value))).packed


def _capture_path(db: sqlite3.Connection, file_id: int) -> str | None:
    row = db.execute(
        "SELECT path FROM facts_project_provenance WHERE file_id=?", (file_id,)
    ).fetchone()
    return None if row is None else str(row[0])


def load_expression_occurrences(
    db: sqlite3.Connection, files: FileResolver
) -> list[Row]:
    sql = (
        "SELECT e.*, "
        "o.usr AS owner_usr,o.qualified_name AS owner_name, "
        "t.usr AS target_usr,t.qualified_name AS target_name "
        "FROM expression_occurrence e "
        "LEFT JOIN symbol o ON o.id=e.owner_id "
        "LEFT JOIN symbol t ON t.id=e.target_id "
        "ORDER BY e.occurrence_id"
    )
    values: list[Row] = []
    for source in db.execute(sql):
        row = dict(source)
        occurrence_id = int(row.pop("occurrence_id"))
        owner = row.get("owner_id")
        target = row.get("target_id")
        row.update(
            {
                "id": occurrence_id,
                "identity": row["identity"],
                "owner_id": None if owner is None else _symbol_id(owner),
                "owner": row.pop("owner_name"),
                "target_id": None if target is None else _symbol_id(target),
                "target": row.pop("target_name"),
                "file": files.path(int(row["file_id"]), required=False),
                "_capture_path": _capture_path(db, int(row["file_id"])),
                "_db_id": occurrence_id,
                "_key": logical_id("expression_occurrence", occurrence_id),
                "_view": "expression_occurrence",
            }
        )
        values.append(row)
    return values


def load_source_regions(db: sqlite3.Connection, files: FileResolver) -> list[Row]:
    sql = (
        "SELECT r.*,s.usr AS symbol_usr,s.qualified_name AS symbol_name "
        "FROM source_region r LEFT JOIN symbol s ON s.id=r.symbol_id "
        "ORDER BY r.region_id"
    )
    values: list[Row] = []
    for source in db.execute(sql):
        row = dict(source)
        region_id = int(row.pop("region_id"))
        row.update(
            {
                "id": region_id,
                "symbol_id": _symbol_id(row["symbol_id"]),
                "symbol": row.pop("symbol_name"),
                "file": files.path(int(row["file_id"]), required=False),
                "_capture_path": _capture_path(db, int(row["file_id"])),
                "_db_id": region_id,
                "_key": logical_id("source_region", region_id),
                "_view": "source_region",
            }
        )
        values.append(row)
    return values
