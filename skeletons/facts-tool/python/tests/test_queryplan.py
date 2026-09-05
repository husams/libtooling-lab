import pytest

from facts_tool import FactsToolError
from facts_tool.queryplan import (
    all_of,
    canonical_json,
    codebase,
    count,
    eq,
    nodes,
    out,
    rank,
    select,
    start,
    symbol,
    validate,
)


def test_pipeline_is_immutable_and_canonical() -> None:
    prefix = start(symbol("app::run")) | out("calls")
    projected = prefix | select(("name",))
    assert len(prefix.plan.stages) == 1
    assert len(projected.plan.stages) == 2
    assert canonical_json(projected.plan) == canonical_json(projected.plan)


def test_empty_all_predicate_is_serializable() -> None:
    plan = (start(codebase()) | nodes(all_of(())) | count()).plan
    validate(plan)
    assert '"kids":[]' in canonical_json(plan)


def test_invalid_depth_fails_stably() -> None:
    plan = (start(symbol("x")) | out("calls", 1, 33)).plan
    with pytest.raises(FactsToolError, match="E_DEPTH"):
        validate(plan)


def test_stage_after_count_is_rejected() -> None:
    plan = (
        start(codebase()) | nodes(eq("kind", "function")) | count() | out("calls")
    ).plan
    with pytest.raises(FactsToolError, match="E_STAGE"):
        validate(plan)


@pytest.mark.parametrize("low,high", ((0, 1), (2, 1), (1, 33)))
def test_invalid_depth_windows_fail_stably(low: int, high: int) -> None:
    with pytest.raises(FactsToolError, match="E_DEPTH"):
        validate((start(symbol("x")) | out("calls", low, high)).plan)


def test_rank_requires_a_path() -> None:
    with pytest.raises(FactsToolError, match="E_STAGE"):
        validate((start(symbol("x")) | rank()).plan)
