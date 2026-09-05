import os
import sqlite3
from pathlib import Path
from urllib.parse import quote

from .errors import FactsToolError, fail


def canonical_path(value: str | os.PathLike[str], role: str) -> Path:
    path = Path(value).expanduser()
    if not path.exists():
        fail("E_DATABASE", f"{role} database does not exist: {path}")
    if not path.is_file():
        fail("E_DATABASE", f"{role} database is not a file: {path}")
    try:
        return path.resolve(strict=True)
    except OSError as exc:
        raise FactsToolError("E_DATABASE", f"cannot resolve {role} database") from exc


def open_readonly(path: Path, role: str) -> sqlite3.Connection:
    uri = f"file:{quote(str(path), safe='/')}?mode=ro&immutable=1"
    connection: sqlite3.Connection | None = None
    try:
        connection = sqlite3.connect(uri, uri=True)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA query_only=ON")
        connection.execute("SELECT count(*) FROM sqlite_master").fetchone()
        return connection
    except sqlite3.Error as exc:
        if connection is not None:
            connection.close()
        raise FactsToolError(
            "E_DATABASE", f"cannot open {role} database: {path}"
        ) from exc


def open_pair(
    facts: str | os.PathLike[str], project: str | os.PathLike[str]
) -> tuple[Path, Path, sqlite3.Connection, sqlite3.Connection]:
    facts_path = canonical_path(facts, "facts")
    project_path = canonical_path(project, "project")
    if os.path.samefile(facts_path, project_path):
        fail("E_DATABASE_ROLE", "facts and project databases must be different files")
    facts_db = open_readonly(facts_path, "facts")
    try:
        project_db = open_readonly(project_path, "project")
    except Exception:
        facts_db.close()
        raise
    return facts_path, project_path, facts_db, project_db
