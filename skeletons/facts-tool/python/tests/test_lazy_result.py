from collections.abc import Iterator
from pathlib import Path

from facts_tool import open_codebase
from facts_tool.provenance import PairProvenance
from facts_tool.result import Result
from facts_tool.rows import Row
from facts_tool.state import ExecutionState


def stream(provenance: PairProvenance, events: list[object]) -> Result:
    def factory() -> tuple[Iterator[Row], ExecutionState]:
        events.append("opened")
        state = ExecutionState()

        def rows() -> Iterator[Row]:
            try:
                for value in range(3):
                    events.append(value)
                    yield {"id": value, "name": str(value), "_key": str(value)}
                state.truncated, state.cursor = True, "2"
                state.partial, state.unknown = True, True
            finally:
                events.append("closed")

        return rows(), state

    return Result.lazy("nodes", "symbol", factory, provenance)


def test_iteration_defers_work_and_closes_partial_stream(
    paired_databases: tuple[Path, Path],
) -> None:
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        events: list[object] = []
        result = stream(cb.provenance, events)
        iterator = iter(result)
        assert events == []
        assert next(iterator)["id"] == 0
        assert events == ["opened", 0]
        iterator.close()
        assert events == ["opened", 0, "closed"]
        assert [row["id"] for row in result] == [0, 1, 2]
        assert result.truncated and result.cursor == "2"
        assert result.partial and result.unknown
        assert events.count("opened") == 2
        assert result._data.values is None


def test_materialization_replays_then_caches_and_serializes(
    paired_databases: tuple[Path, Path],
) -> None:
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        events: list[object] = []
        result = stream(cb.provenance, events)
        assert result.materialize() is result
        assert len(result) == 3
        assert result.nodes is result.values
        assert result.rows == result.paths == ()
        assert [row["id"] for row in result] == [0, 1, 2]
        assert events.count("opened") == 1
        assert result.to_dict()["nodes"][0] == {"id": 0, "name": "0"}
        assert '"cursor": "2"' in result.to_json()


def test_metadata_forces_complete_evaluation(
    paired_databases: tuple[Path, Path],
) -> None:
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        events: list[object] = []
        result = stream(cb.provenance, events)
        assert result.truncated
        assert events == ["opened", 0, 1, 2, "closed"]
        assert len(result.values) == 3
        assert events.count("opened") == 1
