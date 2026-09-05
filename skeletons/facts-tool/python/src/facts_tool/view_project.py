import os
import sqlite3
from pathlib import Path

from .paths import FileResolver
from .rows import Row


def _rows(db: sqlite3.Connection, table: str) -> list[Row]:
    result = []
    for source in db.execute(f'SELECT * FROM "{table}" ORDER BY id'):
        row = dict(source)
        row.update({"_key": f"{table}:{row['id']}", "_view": table})
        result.append(row)
    return result


def load_project(db: sqlite3.Connection, view: str, files: FileResolver) -> list[Row]:
    rows = _rows(db, view)
    if view == "file":
        for row in rows:
            row["path"] = files.path(int(row["id"]))
            row["indexed"] = bool(row["indexed"])
            row["args_overridden"] = bool(row["args_overridden"])
    elif view == "directory":
        for row in rows:
            row["name"] = Path(str(row["path"])).name
    elif view == "clone":
        for row in rows:
            row["path"] = os.path.abspath(str(row["path"]))
            row["name"] = row["label"] or Path(str(row["path"])).name
    return rows
