from collections.abc import Callable, Iterator
from typing import TYPE_CHECKING

from .entity import make_entity
from .queryplan.stages_shape import count
from .result import Result
from .rows import Row
from .state import ExecutionState

if TYPE_CHECKING:
    from .fluent import EntityQuery


def _filtered(
    result: Result, callbacks: tuple[Callable[[Row], bool], ...]
) -> tuple[Iterator[Row], ExecutionState]:
    state = ExecutionState(view=result.view, shape=result.shape)

    def rows() -> Iterator[Row]:
        source = iter(result)
        try:
            for row in source:
                if all(callback(row) for callback in callbacks):
                    yield row
            state.scalar, state.cursor = result.scalar, result.cursor
            state.partial, state.unknown = result.partial, result.unknown
            state.truncated = result.truncated
        finally:
            close = getattr(source, "close", None)
            if close is not None:
                close()

    return rows(), state


def run(query: "EntityQuery", *, lazy: bool | None = None) -> Result:
    result = query.executor.run(query.plan, lazy=lazy)
    if not query._callbacks:
        return result
    filtered = Result.lazy(
        result.shape,
        result.view,
        lambda: _filtered(result, query._callbacks),
        result.provenance,
    )
    return (
        filtered
        if (query.executor.lazy if lazy is None else lazy)
        else (filtered.materialize())
    )


def iter_rows(query: "EntityQuery", *, lazy: bool | None = None) -> Iterator[object]:
    from .graph import GraphQuery

    result = run(query, lazy=lazy)
    graph = GraphQuery(query.executor)
    source = iter(result)
    try:
        for row in source:
            yield make_entity(row, graph) if result.shape == "nodes" else row
    finally:
        close = getattr(source, "close", None)
        if close is not None:
            close()


def all_rows(query: "EntityQuery") -> list[object]:
    return list(iter_rows(query, lazy=False))


def names(query: "EntityQuery") -> list[str]:
    return [str(item["name"]) for item in run(query, lazy=False).values]


def count_rows(query: "EntityQuery") -> int | None:
    return query.executor.run((query._query | count()).plan).scalar


def first(query: "EntityQuery") -> object | None:
    rows = query.limit(1).all()
    return rows[0] if rows else None
