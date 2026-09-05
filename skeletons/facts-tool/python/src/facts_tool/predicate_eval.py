import fnmatch

from .queryplan.types import Pred
from .rows import Row
from .traversal import NeighborFn, traverse

Truth = bool | None


def _field(row: Row, name: str) -> object:
    aliases = {"qual_name": "qualified_name", "column": "col"}
    return row.get(aliases.get(name, name), _MISSING)


def _boolean(op: str, values: list[Truth]) -> Truth:
    if op == "all_of":
        return False if False in values else (None if None in values else True)
    return True if True in values else (None if None in values else False)


def evaluate(pred: Pred, row: Row, neighbors: NeighborFn, budget: int) -> Truth:
    if pred.op in {"all_of", "any_of"}:
        return _boolean(
            pred.op, [evaluate(child, row, neighbors, budget) for child in pred.kids]
        )
    if pred.op == "not":
        value = evaluate(pred.kids[0], row, neighbors, budget)
        return None if value is None else not value
    if pred.op in {"eq", "ne", "glob", "in"}:
        field_value = _field(row, pred.field)
        if field_value is _MISSING:
            return None
        if pred.op == "eq":
            return bool(field_value == pred.value)
        if pred.op == "ne":
            return bool(field_value != pred.value)
        if pred.op == "glob":
            return field_value is not None and fnmatch.fnmatchcase(
                str(field_value), str(pred.value)
            )
        return bool(field_value in pred.value)
    related, truncated = traverse(
        [row],
        pred.relation,
        pred.min_depth,
        pred.max_depth,
        pred.inbound,
        neighbors,
        budget,
    )
    values = [
        True if pred.target is None else evaluate(pred.target, item, neighbors, budget)
        for item in related
    ]
    uncertain = truncated or None in values
    count = sum(value is True for value in values)
    if pred.op == "exists":
        return True if count else (None if uncertain else False)
    if pred.op == "none":
        return False if count else (None if uncertain else True)
    if pred.op == "all":
        return False if False in values else (None if uncertain else True)
    if pred.op == "at_least":
        return True if count >= pred.threshold else (None if uncertain else False)
    if pred.op == "exactly":
        return None if uncertain else count == pred.threshold
    return None


_MISSING = object()
