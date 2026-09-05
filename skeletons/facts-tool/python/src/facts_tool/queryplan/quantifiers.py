from .normalization import normalize_relation
from .types import Pred


def _relation(
    op: str,
    relation: str,
    target: Pred | None,
    min_depth: int,
    max_depth: int,
    inbound: bool,
    threshold: int = 0,
) -> Pred:
    return Pred(
        op,
        relation=normalize_relation(relation),
        target=target,
        min_depth=min_depth,
        max_depth=max_depth,
        inbound=inbound,
        threshold=threshold,
    )


def exists(
    relation: str,
    target: Pred | None = None,
    min_depth: int = 1,
    max_depth: int = 1,
    inbound: bool = False,
) -> Pred:
    return _relation("exists", relation, target, min_depth, max_depth, inbound)


def none(
    relation: str,
    target: Pred | None = None,
    min_depth: int = 1,
    max_depth: int = 1,
    inbound: bool = False,
) -> Pred:
    return _relation("none", relation, target, min_depth, max_depth, inbound)


def all(
    relation: str,
    target: Pred | None = None,
    min_depth: int = 1,
    max_depth: int = 1,
    inbound: bool = False,
) -> Pred:
    return _relation("all", relation, target, min_depth, max_depth, inbound)


def at_least(
    threshold: int,
    relation: str,
    target: Pred | None = None,
    min_depth: int = 1,
    max_depth: int = 1,
    inbound: bool = False,
) -> Pred:
    return _relation(
        "at_least", relation, target, min_depth, max_depth, inbound, threshold
    )


def exactly(
    threshold: int,
    relation: str,
    target: Pred | None = None,
    min_depth: int = 1,
    max_depth: int = 1,
    inbound: bool = False,
) -> Pred:
    return _relation(
        "exactly", relation, target, min_depth, max_depth, inbound, threshold
    )
