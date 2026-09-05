from pytest_bdd import then, when

from facts_tool.queryplan import all as all_related
from facts_tool.queryplan import (
    at_least,
    eq,
    exactly,
    exists,
    none,
    out,
    start,
    symbol,
    where,
)

from .matrix import run_matrix


@when("I evaluate every relationship quantifier")
def quantifiers(cb, world):
    preds = (
        exists("calls"),
        none("overrides"),
        all_related("calls", eq("kind", "function")),
        at_least(2, "calls", max_depth=2),
        exactly(1, "calls"),
    )
    world["quantifiers"] = run_matrix(
        cb,
        lambda database, _: [
            len(database.executor.run((start(symbol("app::run")) | where(pred)).plan))
            == 1
            for pred in preds
        ],
    )


@then("all quantifier expectations hold")
def quantifier_results(world):
    assert all(all(values) for _, values in world["quantifiers"])


@when("I traverse calls from run through depth two")
def traverse_calls(cb, world):
    query = start(symbol("app::run")) | out("calls", 1, 2)
    world["names"] = run_matrix(
        cb,
        lambda database, _: [row["name"] for row in database.executor.run(query.plan)],
    )
