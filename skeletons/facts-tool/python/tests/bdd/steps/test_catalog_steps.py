from pytest_bdd import scenarios, then, when

from facts_tool.catalog import RELATION_NAMES
from facts_tool.queryplan import codebase, eq, nodes, out, start, symbol, view

from .catalog_evidence_steps import *  # noqa: F403

scenarios("../features/catalog.feature")


@then("all 23 persisted relation mappings are available")
def relations_available(cb):
    assert len(RELATION_NAMES) == 23
    assert RELATION_NAMES[0] == "calls"
    assert RELATION_NAMES[-1] == "template_argument_type"
    for relation in RELATION_NAMES:
        assert cb.executor.run(
            (start(symbol("fixture::relationSource")) | out(relation)).plan
        ).nodes


@when("I query run parameters")
def parameters(cb, world):
    result = cb.executor.run((start(symbol("app::run")) | out("has_parameter")).plan)
    world["parameter"] = result.nodes[0]


@then("the stored parameter and default are preserved")
def parameter_result(world):
    row = world["parameter"]
    assert (row["position"], row["name"], row["default_expression"]) == (0, "box", "{}")


@when("I query template slots and supplied arguments")
def templates(cb, world):
    slot = start(symbol("app::Box")) | out("has_template_parameter")
    arg = start(symbol("app::Box<int>")) | out("has_template_argument")
    world["templates"] = (
        cb.executor.run(slot.plan).nodes[0],
        cb.executor.run(arg.plan).nodes[0],
    )


@then("physical template names are mapped conventionally")
def template_result(world):
    slot, argument = world["templates"]
    assert slot["_view"] == "template_parameter" and slot["name"] == "T"
    assert argument["_view"] == "template_argument" and argument["type_id"] == 1


@when("I query project files and include edges")
def project_files(cb, world):
    files = start(codebase()) | view("file") | nodes()
    include = (
        start(codebase())
        | view("file")
        | nodes(eq("name", "main.cpp"))
        | out("includes")
    )
    world["project"] = (
        cb.executor.run(files.plan).nodes,
        cb.executor.run(include.plan).nodes,
    )


@then("both project metadata and facts include data are used")
def project_result(world):
    files, included = world["project"]
    assert all(row["driver"] == "clang++" for row in files)
    assert [row["name"] for row in included] == ["save.cpp"]
    assert "checkout λ with spaces" in files[0]["path"]


@when("I explain a call query")
def explain_query(cb, world):
    world["explain"] = cb.executor.explain(
        (start(symbol("app::run")) | out("calls")).plan
    )


@then("both database identities budgets and catalogs are reported")
def explain_result(world):
    value = world["explain"]
    assert value["provenance"]["facts"]["schema"]["user_version"] == 10
    assert value["budgets"]["max_depth"] == 32
    assert len(value["relations"]) == 23 and "file" in value["views"]
