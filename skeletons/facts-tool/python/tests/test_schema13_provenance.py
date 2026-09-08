import sqlite3

import pytest

from facts_tool import FactsToolError, open_codebase


def test_schema13_missing_capture_provenance_is_unknown(schema13_pair):
    facts, project, _ = schema13_pair
    with sqlite3.connect(facts) as database:
        database.execute("DELETE FROM facts_project_provenance WHERE file_id=1")
    with open_codebase(facts_db=facts, project_db=project) as cb:
        regions = cb.source_regions("app::run", include_text=True)
        expressions = cb.expression_occurrences("app::run")
        assert regions.unknown and expressions.unknown
        assert regions.rows[0]["freshness"] == "unavailable"
        assert expressions.rows[0]["freshness"] == "unavailable"
        assert "provenance" in regions.rows[0]["unavailable_reason"]
        assert "provenance" in expressions.rows[0]["unavailable_reason"]


def test_schema13_malformed_capture_provenance_is_typed(schema13_pair):
    facts, project, _ = schema13_pair
    with sqlite3.connect(facts) as database:
        database.execute(
            "ALTER TABLE facts_project_provenance RENAME TO malformed_provenance"
        )
        database.execute(
            "CREATE TABLE facts_project_provenance(file_id INTEGER PRIMARY KEY, "
            "universe_key TEXT NOT NULL)"
        )
    with pytest.raises(FactsToolError, match="facts_project_provenance") as error:
        open_codebase(facts_db=facts, project_db=project)
    assert error.value.code == "E_SCHEMA"
