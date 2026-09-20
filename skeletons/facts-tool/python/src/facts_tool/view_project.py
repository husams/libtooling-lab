import os
import sqlite3
from collections.abc import Generator, Iterator
from contextlib import closing
from pathlib import Path

from .paths import FileResolver
from .rows import Row


def _rows(db: sqlite3.Connection, table: str) -> Generator[Row, None, None]:
    with closing(db.execute(f'SELECT * FROM "{table}" ORDER BY id')) as sources:
        for source in sources:
            row = dict(source)
            row.update({"_key": f"{table}:{row['id']}", "_view": table})
            yield row


def iter_project(
    db: sqlite3.Connection, view: str, files: FileResolver
) -> Iterator[Row]:
    with closing(_rows(db, view)) as sources:
        for row in sources:
            if view == "file":
                row["path"] = files.path(int(row["id"]))
                row["indexed"] = bool(row["indexed"])
                row["args_overridden"] = bool(row["args_overridden"])
            elif view == "directory":
                row["name"] = Path(str(row["path"])).name
            elif view == "clone":
                row["path"] = os.path.abspath(str(row["path"]))
                row["name"] = row["label"] or Path(str(row["path"])).name
            yield row
