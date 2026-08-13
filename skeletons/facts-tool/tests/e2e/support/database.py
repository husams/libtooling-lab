from __future__ import annotations

import sqlite3
from collections import defaultdict
from collections.abc import Sequence
from pathlib import Path
from typing import Any


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def query(database: Path, sql: str, parameters: Sequence[Any] = ()) -> list[tuple]:
    with sqlite3.connect(database) as connection:
        return connection.execute(sql, parameters).fetchall()


def scalar(database: Path, sql: str, parameters: Sequence[Any] = ()) -> Any:
    rows = query(database, sql, parameters)
    require(len(rows) == 1 and len(rows[0]) == 1, f"not a scalar query: {sql}")
    return rows[0][0]


def table_names(database: Path) -> set[str]:
    return {
        name
        for (name,) in query(
            database,
            "SELECT name FROM sqlite_master "
            "WHERE type='table' AND name NOT LIKE 'sqlite_%'",
        )
    }


def file_snapshot(database: Path) -> list[tuple]:
    return query(database, "SELECT id,path FROM file ORDER BY id")


def symbol_snapshot(database: Path) -> list[tuple]:
    return query(
        database,
        "SELECT id,file_id,file_index,node,usr,qualified_name "
        "FROM symbol ORDER BY file_id,file_index",
    )


def parameters_by_function(database: Path) -> dict[str, list[tuple]]:
    parameters = query(
        database,
        "SELECT s.qualified_name,p.position,p.name,p.type,p.has_default "
        "FROM parameter p JOIN symbol s ON s.id=p.symbol_id "
        "ORDER BY s.qualified_name,p.position",
    )
    result: dict[str, list[tuple]] = defaultdict(list)
    for function, position, name, type_id, has_default in parameters:
        result[function].append((position, name, type_id, has_default))
    return result
