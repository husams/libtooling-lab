import os
import sqlite3
from pathlib import Path
from typing import cast

from .errors import fail


class FileResolver:
    def __init__(self, project: sqlite3.Connection):
        self.project = project

    def row(self, file_id: int) -> sqlite3.Row | None:
        value = self.project.execute(
            "SELECT f.*,d.path AS directory_path,c.path AS component_path,"
            "c.version,c.repository_id,r.active_clone_id,cl.path AS clone_path "
            "FROM file f JOIN directory d ON d.id=f.directory_id "
            "JOIN component c ON c.id=d.component_id "
            "LEFT JOIN repository r ON r.id=c.repository_id "
            "LEFT JOIN clone cl ON cl.id=r.active_clone_id WHERE f.id=?",
            (file_id,),
        ).fetchone()
        return cast(sqlite3.Row | None, value)

    def path(self, file_id: int, *, required: bool = True) -> str | None:
        if file_id == 0:
            return None
        row = self.row(file_id)
        if row is None:
            if required:
                fail("E_IDENTITY", f"FileId {file_id} is absent from project database")
            return None
        component = Path(str(row["component_path"]))
        clone = row["clone_path"]
        anchored = row["repository_id"] is not None and not component.is_absolute()
        anchored = bool(
            anchored
            and clone
            and "<" not in str(component)
            and "$" not in str(component)
        )
        root = Path(str(clone)) / component if anchored else component
        if row["version"]:
            root /= str(row["version"])
        value = root / str(row["directory_path"]) / str(row["name"])
        return os.path.abspath(value)
