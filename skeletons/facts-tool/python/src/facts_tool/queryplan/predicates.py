from collections.abc import Sequence
from typing import Any

from .types import Pred


def all_of(preds: Sequence[Pred]) -> Pred:
    return Pred("all_of", kids=tuple(preds))


def any_of(preds: Sequence[Pred]) -> Pred:
    return Pred("any_of", kids=tuple(preds))


def not_(pred: Pred) -> Pred:
    return Pred("not", kids=(pred,))


def eq(field_name: str, value: Any) -> Pred:
    return Pred("eq", field=field_name, value=value)


def ne(field_name: str, value: Any) -> Pred:
    return Pred("ne", field=field_name, value=value)


def glob(field_name: str, pattern: str) -> Pred:
    return Pred("glob", field=field_name, value=pattern)


def in_list(field_name: str, values: Sequence[Any]) -> Pred:
    return Pred("in", field=field_name, value=tuple(values))
