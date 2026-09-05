from collections.abc import Callable

from .rows import Row, row_key

NeighborFn = Callable[[list[Row], str, bool], list[Row]]


def traverse(
    start: list[Row],
    relation: str,
    low: int,
    high: int,
    inbound: bool,
    neighbors: NeighborFn,
    budget: int,
) -> tuple[list[Row], bool]:
    frontier = list(start)
    found: dict[str, Row] = {}
    expanded = 0
    for depth in range(1, high + 1):
        if expanded >= budget:
            return list(found.values()), True
        next_rows = neighbors(frontier, relation, inbound)
        expanded += len(frontier) + len(next_rows)
        frontier = list({row_key(row): row for row in next_rows}.values())
        if depth >= low:
            found.update((row_key(row), row) for row in frontier)
        if not frontier:
            break
    return list(found.values()), expanded > budget
