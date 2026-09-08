import sqlite3

import pytest

from facts_tool import Budgets, FactsToolError, open_codebase


def test_schema13_expression_paging_is_deterministic(schema13_pair):
    facts, project, _ = schema13_pair
    with open_codebase(facts_db=facts, project_db=project) as cb:
        first = cb.expression_occurrences(limit=1)
        assert first.truncated and first.cursor == "1"
        second = cb.expression_occurrences(after_id=first.cursor)
        assert [row["id"] for row in second] == [2]


def test_field_writers_filter_before_budget_page(schema13_pair):
    facts, project, _ = schema13_pair
    with sqlite3.connect(facts) as db:
        db.execute(
            "UPDATE expression_occurrence SET access='unknown' WHERE occurrence_id=1"
        )
        db.execute(
            "UPDATE expression_occurrence SET access='write' WHERE occurrence_id=2"
        )
    with open_codebase(
        facts_db=facts, project_db=project, budgets=Budgets(result_cap=1)
    ) as cb:
        assert [row["id"] for row in cb.field_writers("app::Box::value")] == [2]


def test_source_regions_page_before_excluded_oversized_row(schema13_pair):
    facts, project, source = schema13_pair
    with sqlite3.connect(facts) as db:
        digest = __import__("hashlib").sha256(source.read_bytes()).hexdigest()
        db.execute(
            "INSERT INTO source_region VALUES(2,'region-2',?,?,?,?,?,?,?,?,?,?)",
            (1 << 32 | 1, 1, 0, 0, 0, 10000, digest, "function", "current", None),
        )
        db.execute(
            "INSERT INTO source_region VALUES(3,'region-3',?,?,?,?,?,?,?,?,?,?)",
            (
                1 << 32 | 1,
                1,
                0,
                0,
                0,
                source.stat().st_size,
                digest,
                "function",
                "current",
                None,
            ),
        )
    with open_codebase(facts_db=facts, project_db=project) as cb:
        first = cb.source_regions("app::run", include_text=True, max_bytes=30, limit=1)
        assert [row["id"] for row in first] == [1]
        assert first.truncated and first.cursor == "1"
        with pytest.raises(FactsToolError, match="max_bytes"):
            cb.source_regions(
                "app::run", include_text=True, max_bytes=30, after_id=first.cursor
            )
        third = cb.source_regions(
            "app::run", include_text=True, max_bytes=30, after_id=2
        )
        assert [row["id"] for row in third] == [3]


def test_source_regions_cursor_continuation_validates_only_selected_page(schema13_pair):
    facts, project, source = schema13_pair
    with sqlite3.connect(facts) as db:
        digest = __import__("hashlib").sha256(source.read_bytes()).hexdigest()
        db.execute(
            "INSERT INTO source_region VALUES(2,'region-2',?,?,?,?,?,?,?,?,?,?)",
            (1 << 32 | 1, 1, 0, 0, 0, 10000, digest, "function", "current", None),
        )
    with open_codebase(facts_db=facts, project_db=project) as cb:
        page = cb.source_regions("app::run", include_text=False, limit=1)
        assert [row["id"] for row in page] == [1]
        assert [
            row["id"] for row in cb.source_regions("app::run", after_id=page.cursor)
        ] == [2]
