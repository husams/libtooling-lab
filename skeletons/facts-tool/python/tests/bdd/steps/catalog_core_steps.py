from pytest_bdd import then, when

from facts_tool.catalog import RELATION_NAMES
from facts_tool.queryplan import out, start, symbol

from .matrix import run_matrix


@then("all 23 persisted relation mappings are available")
def relations_available(cb):
    assert len(RELATION_NAMES) == 23

    def query(database, native):
        source = "app::run" if native else "fixture::relationSource"
        results = [
            database.executor.run((start(symbol(source)) | out(name)).plan)
            for name in RELATION_NAMES
        ]
        assert native or all(result.nodes for result in results)

    run_matrix(cb, query)


@when("I query run parameters")
def parameters(cb, world):
    query = (start(symbol("app::run")) | out("has_parameter")).plan
    world["parameter"] = run_matrix(
        cb, lambda database, _: database.executor.run(query).nodes[0]
    )


@then("the stored parameter and default are preserved")
def parameter_result(world):
    for _, row in world["parameter"]:
        assert (row["position"], row["name"]) == (0, "box")
        assert row["default_expression"].endswith("{}")


@when("I query template slots and supplied arguments")
def templates(cb, world):
    def query(database, native):
        slot = start(symbol("app::Box")) | out("has_template_parameter")
        instance = "c:@N@app@S@Box>#I#p1VI7" if native else "app::Box<int>"
        arg = start(symbol(instance)) | out("has_template_argument")
        return database.executor.run(slot.plan).nodes[0], database.executor.run(
            arg.plan
        ).nodes[0]

    world["templates"] = run_matrix(cb, query)


@then("physical template names are mapped conventionally")
def template_result(world):
    for _, (slot, argument) in world["templates"]:
        assert slot["_view"] == "template_parameter" and slot["name"] == "T"
        assert argument["_view"] == "template_argument" and argument["type_id"] > 0
