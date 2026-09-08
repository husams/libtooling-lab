import sqlite3

from facts_tool import Budgets, open_codebase


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
