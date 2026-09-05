import sqlite3

from .catalog_relations import RELATION_NAMES
from .ids import SymbolId, logical_id
from .paths import FileResolver
from .rows import Row


def _relation_identity(row: sqlite3.Row) -> str:
    source = SymbolId.unpack(int(row["source_id"])).packed
    target = SymbolId.unpack(int(row["destination_id"])).packed
    return f"{source}:{target}:{row['kind']}:{row['position']}"


def load_edges(db: sqlite3.Connection) -> list[Row]:
    result = []
    for source in db.execute(
        "SELECT * FROM relation ORDER BY source_id,kind,position,destination_id"
    ):
        row = dict(source)
        key = logical_id("edge", _relation_identity(source))
        kind = int(row["kind"])
        row.update(
            {
                "id": key,
                "source_id": SymbolId.unpack(int(row["source_id"])).packed,
                "destination_id": SymbolId.unpack(int(row["destination_id"])).packed,
                "kind_id": kind,
                "kind": RELATION_NAMES[kind - 1],
                "_key": key,
                "_view": "edge",
            }
        )
        for field in ("is_virtual_base", "is_implicit", "is_lexical"):
            row[field] = bool(row[field])
        result.append(row)
    return result


def load_sites(db: sqlite3.Connection, files: FileResolver) -> list[Row]:
    result = []
    sql = "SELECT * FROM relation_site ORDER BY source_id,kind,position,file_id,offset"
    for source in db.execute(sql):
        row = dict(source)
        base = _relation_identity(source)
        key = logical_id("site", f"{base}:{row['file_id']}:{row['offset']}")
        kind = int(row["kind"])
        row.update(
            {
                "id": key,
                "source_id": SymbolId.unpack(int(row["source_id"])).packed,
                "destination_id": SymbolId.unpack(int(row["destination_id"])).packed,
                "kind_id": kind,
                "kind": RELATION_NAMES[kind - 1],
                "file": files.path(int(row["file_id"])),
                "_key": key,
                "_view": "site",
            }
        )
        if row["receiver_type_id"] is not None:
            row["receiver_type_id"] = SymbolId.unpack(
                int(row["receiver_type_id"])
            ).packed
        result.append(row)
    return result
