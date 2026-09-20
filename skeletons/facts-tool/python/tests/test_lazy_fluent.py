from pathlib import Path

import pytest

from facts_tool import Entity, FactsToolError, open_codebase
from facts_tool.queryplan import codebase, nodes, start, symbol


def test_query_defaults_to_lazy_and_streams_python_filters(
    paired_databases: tuple[Path, Path],
) -> None:
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        seen: list[str] = []

        def callback(row):
            seen.append(row["name"])
            return True

        query = cb.query().nodes().filter(callback)
        result = query.run()
        assert seen == []
        iterator = iter(result)
        next(iterator)
        assert len(seen) == 1
        iterator.close()
        entity_iterator = iter(query)
        assert isinstance(next(entity_iterator), Entity)
        entity_iterator.close()


def test_eager_options_are_available_after_database_close(
    paired_databases: tuple[Path, Path],
) -> None:
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        plan = (start(codebase()) | nodes()).plan
        explicit = cb.executor.run(plan, lazy=False)
        fluent = cb.query().nodes().run(lazy=False)
        filtered = cb.query().nodes().filter(lambda row: True).run(lazy=False)
        all_rows = cb.query().nodes().all()
    assert explicit.values == fluent.values == filtered.values
    assert len(all_rows) == len(explicit.values) > 0
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1], lazy=False
    ) as cb:
        default = cb.query().nodes().run()
        lazy = cb.query().nodes().run(lazy=True)
        assert lazy._data.values is None
    assert default.values == explicit.values


def test_source_errors_are_deferred_until_iteration(
    paired_databases: tuple[Path, Path],
) -> None:
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        plan = start(symbol("missing")).plan
        result = cb.executor.run(plan)
        with pytest.raises(FactsToolError, match="was not found"):
            next(iter(result))
        with pytest.raises(FactsToolError, match="was not found"):
            cb.executor.run(plan, lazy=False)
