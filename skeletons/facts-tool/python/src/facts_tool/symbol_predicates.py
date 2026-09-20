from .queryplan.types import Pred
from .symbol_predicate_values import SqlPredicate, symbol_equality


def _combine(parts: list[SqlPredicate | None], op: str) -> SqlPredicate | None:
    if any(part is None for part in parts):
        return None
    supported = [part for part in parts if part is not None]
    params = tuple(value for _, values in supported for value in values)
    if len(params) > 500:
        return None
    if not supported:
        return ("1" if op == "AND" else "0"), ()
    return "(" + f" {op} ".join(part[0] for part in supported) + ")", params


def compile_symbol_predicate(pred: Pred | None) -> SqlPredicate | None:
    """Compile only filters whose SQL and public-row truth values are identical."""
    if pred is None:
        return "1", ()
    if pred.op in {"all_of", "any_of"}:
        return _combine(
            [compile_symbol_predicate(child) for child in pred.kids],
            "AND" if pred.op == "all_of" else "OR",
        )
    if pred.op == "not" and len(pred.kids) == 1:
        inner = compile_symbol_predicate(pred.kids[0])
        return None if inner is None else (f"NOT ({inner[0]})", inner[1])
    if pred.op in {"eq", "ne"}:
        equality = symbol_equality(pred.field, pred.value)
        if equality is None or pred.op == "eq":
            return equality
        return f"NOT ({equality[0]})", equality[1]
    if pred.op == "in" and isinstance(pred.value, (tuple, list)):
        # Missing fields evaluate to unknown even for an empty membership list.
        if symbol_equality(pred.field, None) is None:
            return None
        return _combine(
            [symbol_equality(pred.field, value) for value in pred.value], "OR"
        )
    return None
