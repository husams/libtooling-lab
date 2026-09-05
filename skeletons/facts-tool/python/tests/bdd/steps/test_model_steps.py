from pytest_bdd import scenarios, then, when

from facts_tool import Callable, Record
from facts_tool.queryplan import codebase, count, eq, nodes, path, select, start, symbol

from .matrix import run_matrix

scenarios("../features/model.feature")


@when("I get callable and record objects")
def typed_objects(cb, world):
    world["objects"] = run_matrix(
        cb, lambda database, _: (database.get("app::run"), database.get("app::Box"))
    )


@then("their persisted kinds and qualifiers are exposed")
def typed_results(world):
    for _, (run, box) in world["objects"]:
        assert isinstance(run, Callable) and run.is_noexcept
        assert isinstance(box, Record) and box.is_definition


@when("I navigate callers callees fields and definitions")
def navigate(cb, world):
    def query(database, native):
        run = database.get("app::run")
        save = database.get("app::save")
        box = database.get("app::Box")
        return run.callees(2), save.callers(), box.fields(), run.definitions()

    world["navigation"] = run_matrix(cb, query)


@then("navigation uses stored facts and project locations")
def navigation_result(world):
    for native, (callees, callers, fields, definitions) in world["navigation"]:
        assert [item.name for item in callees] == ["save", "persist"]
        assert [item.name for item in callers] == ["run"]
        assert [item.name for item in fields] == ["value"]
        assert definitions[0]["file"].endswith("source.cpp" if native else "save.cpp")


@when("I build serializable and local fluent filters")
def fluent(cb, world):
    def query(database, native):
        portable = database.query("app::run").relation("calls", max_depth=2)
        local = portable.filter(lambda row: row["name"] == "save")
        return portable.to_plan(), local.names()

    world["fluent"] = run_matrix(cb, query)


@then("portable stages lower to a plan and callbacks remain local")
def fluent_result(world):
    for _, (plan, names) in world["fluent"]:
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
    world["exports"] = run_matrix(
        cb,
        lambda database, _: [
            database.executor.run(plan.plan).to_dict() for plan in plans
        ],
    )


@then("every shape retains completeness and provenance metadata")
def export_result(world):
    for _, exports in world["exports"]:
        assert [item["shape"] for item in exports] == [
            "nodes",
            "rows",
            "scalar",
            "path",
            "nodes",
        ]
        for item in exports:
            assert {"truncated", "partial", "unknown", "provenance"} <= item.keys()
