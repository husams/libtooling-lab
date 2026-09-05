from pytest_bdd import then, when

from facts_tool.queryplan import all as all_related
from facts_tool.queryplan import (
    all_of,
    any_of,
    at_least,
    codebase,
    eq,
    exactly,
    exists,
    glob,
    in_list,
    ne,
    nodes,
    none,
    not_,
    order_by,
    out,
    select,
    start,
    symbol,
    where,
)


@when("I select defined function names")
def select_functions(cb, world):
    pred = all_of((eq("kind", "function"), eq("is_definition", True)))
    query = start(codebase()) | nodes(pred) | select(("name",)) | order_by(("name",))
    world["names"] = [row["name"] for row in cb.executor.run(query.plan)]


@when("I filter with every comparison and boolean constructor")
def comparisons(cb, world):
    pred = all_of(
        (
            in_list("kind", ("function",)),
            ne("name", "persist"),
            any_of((glob("name", "r*"), eq("name", "save"))),
            not_(eq("is_external", True)),
        )
    )
    query = start(codebase()) | nodes(pred) | select(("name",)) | order_by(("name",))
    world["names"] = [row["name"] for row in cb.executor.run(query.plan)]


@then('the names are "run,save"')
def names_run_save(world):
    assert world["names"] == ["run", "save"]


@then('the names are "save,persist"')
def names_save_persist(world):
    assert world["names"] == ["save", "persist"]


@when("I evaluate every relationship quantifier")
def quantifiers(cb, world):
    preds = (
        exists("calls"),
        none("overrides"),
        all_related("calls", eq("kind", "function")),
        at_least(2, "calls", max_depth=2),
        exactly(1, "calls"),
    )
    world["quantifiers"] = [
        len(cb.executor.run((start(symbol("app::run")) | where(pred)).plan)) == 1
        for pred in preds
    ]


@then("all quantifier expectations hold")
def quantifier_results(world):
    assert all(world["quantifiers"])


@when("I traverse calls from run through depth two")
def traverse_calls(cb, world):
    query = start(symbol("app::run")) | out("calls", 1, 2)
    world["names"] = [row["name"] for row in cb.executor.run(query.plan)]


@when("I traverse diamond and cyclic call graphs")
def traverse_graph_shapes(cb, world):
    diamond = start(symbol("fixture::diamond_source")) | out("calls", 1, 2)
    cycle = start(symbol("fixture::cycle_a")) | out("calls", 1, 4)
    world["graph_shapes"] = (
        [row["id"] for row in cb.executor.run(diamond.plan)],
        [row["id"] for row in cb.executor.run(cycle.plan)],
    )


@then("each reachable identity appears once")
def graph_shape_results(world):
    assert all(len(ids) == len(set(ids)) for ids in world["graph_shapes"])
