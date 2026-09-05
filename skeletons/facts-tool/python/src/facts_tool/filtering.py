from .errors import fail
from .predicate_eval import evaluate
from .queryplan.types import Pred
from .rows import Row
from .traversal import NeighborFn


def filter_rows(
    rows: list[Row], pred: Pred | None, unknown: str, neighbors: NeighborFn, budget: int
) -> tuple[list[Row], bool]:
    if pred is None:
        return rows, False
    result = []
    saw_unknown = False
    for row in rows:
        value = evaluate(pred, row, neighbors, budget)
        if value is None:
            saw_unknown = True
            if unknown == "error":
                fail("E_UNKNOWN", f"predicate evidence is unknown for {row['_key']}")
            if unknown == "include":
                result.append(row)
        elif value:
            result.append(row)
    return result, saw_unknown
