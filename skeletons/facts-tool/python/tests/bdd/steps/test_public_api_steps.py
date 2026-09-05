import pytest
from pytest_bdd import scenarios, then, when

from facts_tool import FactsToolError
from facts_tool.queryplan import codebase, entity, eq, nodes, out, start, symbol

from .matrix import run_matrix
from .public_budget_steps import *  # noqa: F403
from .public_helpers import *  # noqa: F403
from .public_unknown_steps import *  # noqa: F403

scenarios("../features/public-api.feature")


@when("I query codebase symbol and adapted entity sources")
def source_forms(cb, world):
    def query(database, native):
        run = database.executor.run(start(symbol("app::run")).plan).nodes[0]
        refs = (run["usr"], "app::run", "run")
        ids = [
            database.executor.run(start(symbol(ref)).plan).nodes[0]["id"]
            for ref in refs
        ]
        adapted = database.executor.run(start(entity("app::run")).plan).nodes[0]["id"]
        found = database.executor.run(
            (start(codebase()) | nodes(eq("name", "run"))).plan
        )
        return *ids, adapted, found.nodes[0]["id"]

    world["source_ids"] = run_matrix(cb, query)


@then("USR qualified-name and spelling lookups agree")
def source_result(world):
    assert all(len(set(ids)) == 1 for _, ids in world["source_ids"])


@when("I enumerate with a result cap and cursor")
def pagination(cb, world):
    def query(database, native):
        plan = (start(codebase()) | nodes()).plan
        first = database.executor.run(plan, result_cap=2)
        second = database.executor.run(
            plan, after_id=first.nodes[-1]["id"], result_cap=2
        )
        return first, second

    world["pages"] = run_matrix(cb, query)


@then("truncation and the next page are explicit")
def pagination_result(world):
    for _, (first, second) in world["pages"]:
        assert first.truncated and first.cursor is not None
        assert {row["id"] for row in first}.isdisjoint(row["id"] for row in second)


@when("I query a SQL-like source and malformed catalog names")
def invalid_data(cb, world):
    def query(database, native):
        failures = []
        plans = (
            start(symbol("'; DROP TABLE symbol;--")),
            start(codebase()) | nodes(eq("missing_field", 1)),
            start(symbol("app::run")) | out("missing_relation"),
        )
        for plan in plans:
            with pytest.raises(FactsToolError) as raised:
                database.executor.run(plan.plan)
            failures.append(raised.value.code)
        return failures

    world["failure_codes"] = run_matrix(cb, query)


@then("stable source field and relation errors are returned")
def invalid_result(world):
    assert all(
        codes == ["E_SOURCE", "E_FIELD", "E_RELATION"]
        for _, codes in world["failure_codes"]
    )
