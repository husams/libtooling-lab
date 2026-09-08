import shutil
import sqlite3
from pathlib import Path

from facts_tool import FactsToolError, open_codebase


def assert_provenance_outcomes(facts: Path, project: Path, source: Path) -> None:
    with sqlite3.connect(project) as database:
        database.execute(
            "UPDATE clone SET path=? WHERE id=1", (str(source.parent.parent),)
        )
    with sqlite3.connect(facts) as database:
        database.execute("DELETE FROM facts_project_provenance WHERE file_id=1")
    with open_codebase(facts_db=facts, project_db=project) as incomplete:
        assert incomplete.source_regions(include_text=True).unknown
        assert incomplete.expression_occurrences().unknown
    malformed = facts.with_name("malformed-facts.sqlite")
    shutil.copy2(facts, malformed)
    with sqlite3.connect(malformed) as database:
        database.execute(
            "ALTER TABLE facts_project_provenance RENAME TO malformed_provenance"
        )
        database.execute(
            "CREATE TABLE facts_project_provenance(file_id INTEGER PRIMARY KEY, "
            "universe_key TEXT NOT NULL)"
        )
    try:
        open_codebase(facts_db=malformed, project_db=project)
    except FactsToolError as error:
        assert error.code == "E_SCHEMA"
    else:
        raise AssertionError("malformed provenance must be typed")
    with sqlite3.connect(facts) as database:
        database.execute(
            "INSERT OR REPLACE INTO facts_project_provenance VALUES(1,?,'demo')",
            (str(source),),
        )
    with sqlite3.connect(project) as database:
        database.execute(
            "UPDATE clone SET path=? WHERE id=1", (str(source.parent.parent),)
        )
