"""Published facts must resolve to the active clone after checkout switches."""
import sqlite3

from pytest_bdd import then


@then("facts provenance points at the new active checkout")
def current_provenance(watch_catalog):
    file_id = watch_catalog.rows("SELECT id FROM file WHERE name='main.cpp'")[0][0]
    facts = watch_catalog.server.root / "facts.db"
    with sqlite3.connect(facts.as_uri() + "?mode=ro", uri=True) as connection:
        paths = connection.execute(
            "SELECT path FROM facts_project_provenance WHERE file_id=?", [file_id]).fetchall()
    assert paths == [(str(watch_catalog.inactive_source),)], paths
