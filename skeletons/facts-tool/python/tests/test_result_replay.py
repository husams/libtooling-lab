from collections.abc import Iterator
from dataclasses import replace
from pathlib import Path

import pytest

from facts_tool import open_codebase
from facts_tool.result import Result
from facts_tool.rows import Row
from facts_tool.state import ExecutionState


@pytest.fixture
def replayable(paired_databases: tuple[Path, Path]):
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        passes: list[int] = []

        def factory() -> tuple[Iterator[Row], ExecutionState]:
            number = len(passes) + 1
            passes.append(number)
            state = ExecutionState(cursor=str(number), truncated=number == 1)
            return iter(({"pass": number}, {"pass": number})), state

        return Result.lazy("nodes", "symbol", factory, cb.provenance), passes


@pytest.mark.parametrize("materialize", [False, True])
def test_older_iterator_cannot_overwrite_newer_metadata(replayable, materialize):
    result, passes = replayable
    older = iter(result)
    assert next(older) == {"pass": 1}
    newer = result.values if materialize else tuple(iter(result))
    assert newer == ({"pass": 2}, {"pass": 2})
    assert tuple(older) == ({"pass": 1},)
    assert result.cursor == "2" and not result.truncated
    assert passes == [1, 2]
    if materialize:
        assert result.values == newer


def test_list_materializes_only_one_pass(replayable):
    result, passes = replayable
    assert list(result) == [{"pass": 1}, {"pass": 1}]
    assert passes == [1]
    assert result.values == ({"pass": 1}, {"pass": 1})


def test_dataclass_replacement_honors_explicit_values(replayable):
    result, passes = replayable
    changed = replace(result, values=(), truncated=False)
    assert changed.values == () and not changed.truncated
    assert changed._data is None
    assert passes == [1]


def test_failure_closes_stream_without_caching_partial_rows(replayable):
    original, _ = replayable
    events: list[str] = []

    def factory() -> tuple[Iterator[Row], ExecutionState]:
        def rows() -> Iterator[Row]:
            try:
                events.append("opened")
                yield {"id": 1}
                raise ValueError("execution failed")
            finally:
                events.append("closed")

        return rows(), ExecutionState()

    result = Result.lazy("nodes", "symbol", factory, original.provenance)
    with pytest.raises(ValueError, match="execution failed"):
        result.materialize()
    assert result._data.values is None
    assert not result._data.complete
    assert events == ["opened", "closed"]
