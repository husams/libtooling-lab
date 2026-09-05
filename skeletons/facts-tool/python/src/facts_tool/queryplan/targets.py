from collections.abc import Sequence

from .predicates import all_of, any_of, eq
from .quantifiers import exists, none
from .types import Pred, TargetSet


def any_target(refs: Sequence[str]) -> TargetSet:
    return TargetSet("any", tuple(refs))


def all_targets(refs: Sequence[str]) -> TargetSet:
    return TargetSet("all", tuple(refs))


def no_targets(refs: Sequence[str]) -> TargetSet:
    return TargetSet("none", tuple(refs))


def target_ref(ref: str) -> Pred:
    return any_of((eq("usr", ref), eq("qualified_name", ref), eq("spelling", ref)))


def target_set_pred(
    relation: str, targets: TargetSet, inbound: bool = False, max_depth: int = 1
) -> Pred:
    if not targets.refs:
        return any_of(()) if targets.kind == "any" else all_of(())
    parts = tuple(
        exists(relation, target_ref(ref), 1, max_depth, inbound) for ref in targets.refs
    )
    if targets.kind == "any":
        return parts[0] if len(parts) == 1 else any_of(parts)
    if targets.kind == "all":
        return parts[0] if len(parts) == 1 else all_of(parts)
    return none(
        relation,
        any_of(tuple(target_ref(ref) for ref in targets.refs)),
        1,
        max_depth,
        inbound,
    )
