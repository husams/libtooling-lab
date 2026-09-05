import pytest
from pytest_bdd import then, when

from facts_tool import Budgets, FactsToolError, open_codebase
from facts_tool.queryplan import all_of, any_of, exists, start, symbol, where


@when("I evaluate unknown evidence and empty boolean sets")
def unknown_policies(paired_databases, world):
    facts, project = paired_databases
    results = []
    with open_codebase(
        facts_db=facts, project_db=project, budgets=Budgets(traversal=0)
    ) as cb:
        for policy in ("exclude", "include"):
            query = start(symbol("app::run")) | where(exists("calls"), policy)
            results.append((len(cb.executor.run(query.plan)), policy))
        with pytest.raises(FactsToolError) as raised:
            query = start(symbol("app::run")) | where(exists("calls"), "error")
            cb.executor.run(query.plan)
    with open_codebase(facts_db=facts, project_db=project) as cb:
        empty_all = start(symbol("app::run")) | where(all_of(()))
        empty_any = start(symbol("app::run")) | where(any_of(()))
        results.extend(
            (len(cb.executor.run(empty_all.plan)), len(cb.executor.run(empty_any.plan)))
        )
    world["unknown_policies"] = results, raised.value.code


@then("exclude include error and empty-set semantics are explicit")
def unknown_results(world):
    expected = [(0, "exclude"), (1, "include"), 1, 0]
    assert world["unknown_policies"] == (expected, "E_UNKNOWN")
