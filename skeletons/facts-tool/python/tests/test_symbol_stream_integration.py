import sqlite3
from unittest.mock import patch

import pytest

from facts_tool import open_codebase
from facts_tool.budgets import Budgets
from facts_tool.execution_context import ExecutionContext
from facts_tool.executor_dispatch import execute
from facts_tool.queryplan import eq
from facts_tool.view_symbols import _symbol


@pytest.mark.parametrize("lazy", [True, False])
def test_fluent_where_uses_name_index_and_hydrates_only_match(paired_databases, lazy):
    facts, project = paired_databases
    with sqlite3.connect(facts) as db:
        db.execute("CREATE INDEX idx_symbol_qualified_name ON symbol(qualified_name)")
    with open_codebase(facts_db=facts, project_db=project) as cb:
        statements = []
        cb._facts.set_trace_callback(statements.append)
        with patch("facts_tool.view_symbols._symbol", wraps=_symbol) as decode:
            query = cb.query().nodes().where(eq("qualified_name", "app::run"))
            result = query.run(lazy=lazy)
            if lazy:
                assert statements == []
            assert [row["qualified_name"] for row in result] == ["app::run"]
        assert decode.call_count == 1
        cb._facts.set_trace_callback(None)
        plans = [
            str(row[3])
            for sql in statements
            if "s.*" in sql
            for row in cb._facts.execute("EXPLAIN QUERY PLAN " + sql)
        ]
        assert any("idx_symbol_qualified_name" in step for step in plans)


@pytest.mark.parametrize("budget", [1, 2, 3, 100])
@pytest.mark.parametrize("after", [None, 1, (1 << 32) + 1, "symbol:1"])
def test_pushed_filters_preserve_existing_budget_metadata(
    paired_databases, budget, after
):
    facts, project = paired_databases
    with open_codebase(
        facts_db=facts, project_db=project, budgets=Budgets(enumeration=budget)
    ) as cb:
        query = cb.query().nodes(eq("kind", "function")).where(eq("usr", "c:@F@save#"))
        context = ExecutionContext(
            cb.executor.loader, cb.executor.neighbors, cb.executor.budgets, after
        )
        expected = execute(query.plan, context)
        actual = cb.executor.run(query.plan, after_id=after, lazy=False)
        assert actual.values == tuple(expected.values)
        assert actual.cursor == expected.cursor
        assert actual.truncated == expected.truncated
        assert actual.unknown == expected.unknown


def test_pushdown_stops_before_a_limit_stage(paired_databases):
    facts, project = paired_databases
    with open_codebase(facts_db=facts, project_db=project) as cb:
        query = cb.query().nodes().limit(1).where(eq("qualified_name", "app::run"))
        assert tuple(query.run()) == ()
