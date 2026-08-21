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
    rows = query(
        database,
        "SELECT file.id, component.path, component.version, "
        "component.repository_id, clone.path, directory.path, file.name "
        "FROM file JOIN directory ON directory.id=file.directory_id "
        "JOIN component ON component.id=directory.component_id "
        "LEFT JOIN repository ON repository.id=component.repository_id "
        "LEFT JOIN clone ON clone.id=repository.active_clone_id "
        "ORDER BY file.id",
    )
    result = []
    for file_id, component, version, repository_id, clone, directory, name in rows:
        component_path = Path(component)
        effective = component_path / version if version else component_path
        clone_anchored = (
            repository_id is not None
            and not component_path.is_absolute()
            and clone
            and "<" not in component
            and "$" not in component
        )
        root = Path(clone) / effective if clone_anchored else effective
        result.append((file_id, str((root / directory / name).resolve())))
    return result


def symbol_snapshot(database: Path) -> list[tuple]:
    return query(
        database,
        "SELECT id,((id >> 32) & 4294967295),(id & 4294967295),"
        "node,usr,qualified_name FROM symbol ORDER BY id",
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
