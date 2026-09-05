import pytest
from pytest_bdd import scenarios, then, when

from facts_tool import Budgets, FactsToolError, open_codebase
from facts_tool.queryplan import codebase, entity, eq, nodes, out, path, start, symbol

from .public_helpers import *  # noqa: F403
from .public_unknown_steps import *  # noqa: F403

scenarios("../features/public-api.feature")


@when("I query codebase symbol and adapted entity sources")
def source_forms(cb, world):
    refs = ("c:@F@run#", "app::run", "run")
    ids = [cb.executor.run(start(symbol(ref)).plan).nodes[0]["id"] for ref in refs]
    adapted = cb.executor.run(start(entity("app::run")).plan).nodes[0]["id"]
    enumerated = cb.executor.run((start(codebase()) | nodes(eq("name", "run"))).plan)
    world["source_ids"] = (*ids, adapted, enumerated.nodes[0]["id"])


@then("USR qualified-name and spelling lookups agree")
def source_result(world):
    assert len(set(world["source_ids"])) == 1


@when("I enumerate with a result cap and cursor")
def pagination(cb, world):
    plan = (start(codebase()) | nodes()).plan
    first = cb.executor.run(plan, result_cap=2)
    second = cb.executor.run(plan, after_id=first.nodes[-1]["id"], result_cap=2)
    world["pages"] = first, second


@then("truncation and the next page are explicit")
def pagination_result(world):
    first, second = world["pages"]
    assert first.truncated and first.cursor is not None
    assert {row["id"] for row in first}.isdisjoint(row["id"] for row in second)


@when("I traverse with a one-state budget")
def traversal_budget(paired_databases, world):
    with open_codebase(
        facts_db=paired_databases[0],
        project_db=paired_databases[1],
        budgets=Budgets(traversal=1),
    ) as cb:
        query = start(symbol("app::run")) | out("calls", 1, 2)
        world["budget_result"] = cb.executor.run(query.plan)


@then("the result reports truncation")
def budget_result(world):
    assert world["budget_result"].truncated


@when("I reconstruct a path with a one-state budget")
def reconstruction_budget(paired_databases, world):
    with open_codebase(
        facts_db=paired_databases[0],
        project_db=paired_databases[1],
        budgets=Budgets(witness_reconstruction=1),
    ) as cb:
        target = start(symbol("app::persist"))
        query = start(symbol("app::run")) | path(target, "calls")
        world["path_budget_result"] = cb.executor.run(query.plan)


@then("the path result reports truncation")
def reconstruction_result(world):
    result = world["path_budget_result"]
    assert result.truncated and not result.paths


@when("I query a SQL-like source and malformed catalog names")
def invalid_data(cb, world):
    failures = []
    for query in (
        start(symbol("'; DROP TABLE symbol;--")),
        start(codebase()) | nodes(eq("missing_field", 1)),
        start(symbol("app::run")) | out("missing_relation"),
    ):
        with pytest.raises(FactsToolError) as raised:
            cb.executor.run(query.plan)
        failures.append(raised.value.code)
    world["failure_codes"] = failures


@then("stable source field and relation errors are returned")
def invalid_result(world):
    assert world["failure_codes"] == ["E_SOURCE", "E_FIELD", "E_RELATION"]
