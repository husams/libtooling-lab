import sqlite3

from .errors import fail


def validate_pair(facts: sqlite3.Connection, project: sqlite3.Connection) -> None:
    _validate_registry(project)
    known = {int(row[0]) for row in project.execute("SELECT id FROM file")}
    queries = (
        "SELECT DISTINCT ((id >> 32) & 4294967295) FROM symbol",
        "SELECT DISTINCT file_id FROM definition",
        "SELECT DISTINCT file_id FROM relation_site",
        "SELECT src_file_id FROM include_dependency UNION SELECT dst_file_id "
        "FROM include_dependency",
    )
    used: set[int] = set()
    tables = {
        str(row[0])
        for row in facts.execute("SELECT name FROM sqlite_master WHERE type='table'")
    }
    for sql in queries:
        table = sql.split(" FROM ", 1)[1].split()[0]
        if table in tables:
            used.update(int(row[0]) for row in facts.execute(sql) if int(row[0]) != 0)
    missing = sorted(used - known)
    if missing:
        fail("E_DATABASE_PAIR", f"project database lacks FileIds: {missing[:8]}")


def _validate_registry(project: sqlite3.Connection) -> None:
    row = project.execute(
        "SELECT complete,fingerprint,file_count FROM project_registry WHERE id=1"
    ).fetchone()
    if row is None:
        fail("E_DATABASE_PAIR", "project registry mapping is missing")
    actual = int(project.execute("SELECT count(*) FROM file").fetchone()[0])
    if not bool(row[0]):
        fail("E_DATABASE_PAIR", "project registry is incomplete")
    if int(row[2]) != actual:
        fail("E_DATABASE_PAIR", "project registry file count does not match files")
    if not str(row[1]):
        fail("E_DATABASE_PAIR", "project registry fingerprint is missing")
