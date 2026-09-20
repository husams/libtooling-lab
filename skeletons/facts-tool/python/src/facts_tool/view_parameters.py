import sqlite3
from collections.abc import Iterator
from contextlib import closing

from .ids import SymbolId, logical_id
from .paths import FileResolver
from .rows import Row


def iter_parameters(db: sqlite3.Connection, files: FileResolver) -> Iterator[Row]:
    sql = (
        "SELECT p.*,d.expression AS default_expression,d.evaluated_kind AS "
        "default_kind,d.evaluated_value AS default_value FROM parameter p "
        "LEFT JOIN parameter_default d ON d.symbol_id=p.symbol_id AND "
        "d.position=p.position ORDER BY p.symbol_id,p.position"
    )
    with closing(db.execute(sql)) as sources:
        for source in sources:
            owner = SymbolId.unpack(int(source["symbol_id"]))
            row = dict(source)
            key = logical_id("parameter", f"{owner.packed}:{source['position']}")
            row.update(
                {
                    "id": key,
                    "owner_id": owner.packed,
                    "file_id": owner.file_id,
                    "file": files.path(owner.file_id),
                    "_db_id": int(source["symbol_id"]),
                    "_key": key,
                    "_view": "parameter",
                }
            )
            for key in (
                "is_pointer",
                "is_lvalue_reference",
                "is_rvalue_reference",
                "is_forwarding_reference",
                "is_const",
                "is_pack",
                "has_default",
            ):
                row[key] = bool(row[key])
            row["type_id"] = SymbolId.unpack(int(row.pop("type"))).packed
            yield row


def iter_template_parameters(db: sqlite3.Connection) -> Iterator[Row]:
    with closing(
        db.execute("SELECT * FROM template_argument ORDER BY symbol_id,position")
    ) as sources:
        for source in sources:
            owner = SymbolId.unpack(int(source["symbol_id"]))
            row = dict(source)
            key = logical_id("template_parameter", f"{owner.packed}:{row['position']}")
            row.update(
                {
                    "id": key,
                    "owner_id": owner.packed,
                    "_db_id": int(source["symbol_id"]),
                    "_key": key,
                    "_view": "template_parameter",
                }
            )
            for field in ("is_parameter_pack", "is_non_type", "is_template_template"):
                row[field] = bool(row[field])
            row["type_id"] = SymbolId.unpack(int(row["type_id"])).packed
            yield row


def iter_template_arguments(db: sqlite3.Connection) -> Iterator[Row]:
    with closing(
        db.execute("SELECT * FROM template_parameter ORDER BY symbol_id,position")
    ) as sources:
        for source in sources:
            owner = SymbolId.unpack(int(source["symbol_id"]))
            row = dict(source)
            key = logical_id("template_argument", f"{owner.packed}:{row['position']}")
            row.update(
                {
                    "id": key,
                    "owner_id": owner.packed,
                    "_db_id": int(source["symbol_id"]),
                    "_key": key,
                    "_view": "template_argument",
                    "type_id": SymbolId.unpack(int(row["type_id"])).packed,
                }
            )
            for field in (
                "is_pointer",
                "is_lvalue_reference",
                "is_rvalue_reference",
                "is_forwarding_reference",
                "is_const",
                "is_pack",
            ):
                row[field] = bool(row[field])
            yield row
