from pytest_bdd import scenarios, then, when

from facts_tool import Callable, Record
from facts_tool.queryplan import codebase, count, eq, nodes, path, select, start, symbol

scenarios("../features/model.feature")


@when("I get callable and record objects")
def typed_objects(cb, world):
    world["objects"] = (cb.get("app::run"), cb.get("app::Box"))


@then("their persisted kinds and qualifiers are exposed")
def typed_results(world):
    run, box = world["objects"]
    assert isinstance(run, Callable) and run.is_noexcept
    assert isinstance(box, Record) and box.is_definition


@when("I navigate callers callees fields and definitions")
def navigate(cb, world):
    run, save, box = cb.get("app::run"), cb.get("app::save"), cb.get("app::Box")
    world["navigation"] = (
        run.callees(2),
        save.callers(),
        box.fields(),
        run.definitions(),
    )


@then("navigation uses stored facts and project locations")
def navigation_result(world):
    callees, callers, fields, definitions = world["navigation"]
    assert [item.name for item in callees] == ["save", "persist"]
    assert [item.name for item in callers] == ["run"]
    assert [item.name for item in fields] == ["value"]
    assert definitions[0]["file"].endswith("save.cpp")


@when("I build serializable and local fluent filters")
def fluent(cb, world):
    portable = cb.query("app::run").relation("calls", max_depth=2)
    local = portable.filter(lambda row: row["name"] == "save")
    world["fluent"] = (portable.to_plan(), local.names())


@then("portable stages lower to a plan and callbacks remain local")
def fluent_result(world):
    plan, names = world["fluent"]
    assert len(plan.stages) == 1 and names == ["save"]


@when("I export node row scalar path and empty results")
def export_results(cb, world):
    base = start(codebase()) | nodes(eq("kind", "function"))
    plans = (
        base,
        base | select(("name",)),
        base | count(),
        start(symbol("app::run")) | path(start(symbol("app::persist")), "calls"),
        start(codebase()) | nodes(eq("name", "missing")),
    )
    world["exports"] = [cb.executor.run(plan.plan).to_dict() for plan in plans]


@then("every shape retains completeness and provenance metadata")
def export_result(world):
    assert [item["shape"] for item in world["exports"]] == [
        "nodes",
        "rows",
        "scalar",
        "path",
        "nodes",
    ]
    for item in world["exports"]:
        assert {"truncated", "partial", "unknown", "provenance"} <= item.keys()
