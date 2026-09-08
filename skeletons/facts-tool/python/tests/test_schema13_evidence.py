import sqlite3

import pytest

from facts_tool import FactsToolError, open_codebase


def test_schema13_expression_writers_and_bounded_region(schema13_pair):
    facts, project, source = schema13_pair
    with open_codebase(facts_db=facts, project_db=project) as cb:
        assert [row["access"] for row in cb.expression_occurrences("app::run")] == [
            "write",
            "unknown",
        ]
        assert [row["access"] for row in cb.field_writers("app::Box::value")] == [
            "write"
        ]
        region = cb.source_regions("app::run", include_text=True).rows[0]
        assert region["text"] == source.read_text(encoding="utf-8")
        assert region["freshness"] == "current"


def test_schema13_stale_and_legacy_capability(schema13_pair, paired_databases):
    facts, project, source = schema13_pair
    source.write_text("changed\n", encoding="utf-8")
    with open_codebase(facts_db=facts, project_db=project) as cb:
        row = cb.source_regions("app::run", include_text=True).rows[0]
        assert row["freshness"] == "stale" and "text" not in row
        assert cb.source_regions("app::run").rows[0]["freshness"] == "stale"
        assert all(
            row["freshness"] == "stale" for row in cb.expression_occurrences("app::run")
        )
    old_facts, old_project = paired_databases
    with sqlite3.connect(old_facts) as database:
        database.execute("PRAGMA user_version=10")
    with open_codebase(facts_db=old_facts, project_db=old_project) as cb:
        with pytest.raises(FactsToolError) as error:
            cb.expression_occurrences()
        assert error.value.code == "E_CAPABILITY"


def test_schema13_pair_rejects_evidence_file_id(schema13_pair):
    facts, project, _ = schema13_pair
    with sqlite3.connect(facts) as database:
        database.execute("UPDATE source_region SET file_id=99")
    with pytest.raises(FactsToolError, match="FileIds") as error:
        open_codebase(facts_db=facts, project_db=project)
    assert error.value.code == "E_DATABASE_PAIR"


def test_schema13_missing_cross_checkout_and_size_outcomes(schema13_pair, tmp_path):
    facts, project, source = schema13_pair
    with (
        open_codebase(facts_db=facts, project_db=project) as cb,
        pytest.raises(FactsToolError, match="max_bytes"),
    ):
        cb.source_regions("app::run", include_text=True, max_bytes=1)
    source.unlink()
    with open_codebase(facts_db=facts, project_db=project) as cb:
        assert cb.source_regions("app::run", include_text=True).unknown
    with sqlite3.connect(project) as database:
        database.execute("UPDATE clone SET path=?", (str(tmp_path / "other"),))
    with open_codebase(facts_db=facts, project_db=project) as cb:
        assert (
            cb.source_regions("app::run", include_text=True).rows[0]["freshness"]
            == "unavailable"
        )


def test_schema13_same_content_cross_checkout_is_unavailable(schema13_pair, tmp_path):
    facts, project, source = schema13_pair
    other = tmp_path / "other-checkout" / "src"
    other.mkdir(parents=True)
    alternate = other / "main.cpp"
    alternate.write_bytes(source.read_bytes())
    with sqlite3.connect(project) as database:
        database.execute("UPDATE clone SET path=?", (str(other.parent),))
    with open_codebase(facts_db=facts, project_db=project) as cb:
        regions = cb.source_regions("app::run", include_text=True)
        expressions = cb.expression_occurrences("app::run")
        assert regions.unknown and expressions.unknown
        assert regions.rows[0]["freshness"] == "unavailable"
        assert all(row["freshness"] == "unavailable" for row in expressions)
        assert "checkout" in regions.rows[0]["unavailable_reason"]
        assert "checkout" in expressions.rows[0]["unavailable_reason"]
