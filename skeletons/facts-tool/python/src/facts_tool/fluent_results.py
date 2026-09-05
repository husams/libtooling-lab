from typing import TYPE_CHECKING

from .entity import make_entity
from .queryplan.stages_shape import count
from .result import Result

if TYPE_CHECKING:
    from .fluent import EntityQuery


def run(query: "EntityQuery") -> Result:
    result = query.executor.run(query.plan)
    if not query._callbacks:
        return result
    kept = tuple(
        row
        for row in result.values
        if all(callback(row) for callback in query._callbacks)
    )
    return Result(
        result.shape,
        result.view,
        kept,
        result.scalar,
        result.truncated,
        result.partial,
        result.unknown,
        result.cursor,
        result.provenance,
    )


def all_rows(query: "EntityQuery") -> list[object]:
    from .graph import GraphQuery

    result = run(query)
    graph = GraphQuery(query.executor)
    return [
        make_entity(row, graph) if result.shape == "nodes" else row
        for row in result.values
    ]


def names(query: "EntityQuery") -> list[str]:
    return [str(item["name"]) for item in run(query).values]


def count_rows(query: "EntityQuery") -> int | None:
    return query.executor.run((query._query | count()).plan).scalar


def first(query: "EntityQuery") -> object | None:
    rows = query.limit(1).all()
    return rows[0] if rows else None
