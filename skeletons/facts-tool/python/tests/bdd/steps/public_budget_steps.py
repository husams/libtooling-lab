from pytest_bdd import then, when

from facts_tool import Budgets, open_codebase
from facts_tool.queryplan import out, path, start, symbol


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
