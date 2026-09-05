from .normalization import normalize_relation, unknown_value
from .types import Pred, Query, Stage, TraversalMode, UnknownPolicy


def nodes(
    pred: Pred | None = None, unknown: UnknownPolicy | str = UnknownPolicy.EXCLUDE
) -> Stage:
    return Stage("nodes", pred=pred, unknown=unknown_value(unknown))


def view(level: str) -> Stage:
    return Stage("view", level=level)


def where(pred: Pred, unknown: UnknownPolicy | str = UnknownPolicy.EXCLUDE) -> Stage:
    return Stage("where", pred=pred, unknown=unknown_value(unknown))


def out(
    relation: str,
    min_depth: int = 1,
    max_depth: int = 1,
    mode: TraversalMode | str = TraversalMode.STATIC,
) -> Stage:
    value = mode.value if isinstance(mode, TraversalMode) else mode
    return Stage(
        "out",
        relation=normalize_relation(relation),
        min_depth=min_depth,
        max_depth=max_depth,
        mode=value,
    )


def in_(relation: str, min_depth: int = 1, max_depth: int = 1) -> Stage:
    return Stage(
        "in",
        relation=normalize_relation(relation),
        min_depth=min_depth,
        max_depth=max_depth,
    )


def sites() -> Stage:
    return Stage("sites")


def union_(operand: Query) -> Stage:
    return Stage("union", operand=operand.plan)


def intersect(operand: Query) -> Stage:
    return Stage("intersect", operand=operand.plan)


def except_(operand: Query) -> Stage:
    return Stage("except", operand=operand.plan)


def path(
    to: Query,
    relation: str,
    min_depth: int = 1,
    max_depth: int = 8,
    shortest: int = 0,
    inbound: bool = False,
) -> Stage:
    return Stage(
        "path",
        relation=normalize_relation(relation),
        operand=to.plan,
        min_depth=min_depth,
        max_depth=max_depth,
        n=shortest,
        inbound=inbound,
    )


def rank(top_n: int = 0) -> Stage:
    return Stage("rank", n=top_n)


def reverse_type_use(max_depth: int = 8) -> Stage:
    return Stage("reverse_type_use", max_depth=max_depth)
