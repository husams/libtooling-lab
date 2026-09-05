from pytest_bdd import then, when

from facts_tool import FactsToolError
from facts_tool.queryplan import codebase, nodes, out, start, symbol, view


@when("I request devirtualized traversal")
def unavailable_capability(cb, world):
    queries = [start(symbol("app::run")) | out("calls", mode="devirtualized")]
    queries.extend(
        start(codebase()) | view(name) | nodes()
        for name in ("entity", "type", "type_layer", "call_argument")
    )
    results = []
    for _, database in cb:
        codes = []
        for query in queries:
            try:
                database.executor.run(query.plan)
            except FactsToolError as exc:
                codes.append(exc.code)
        try:
            database.executor.explain(queries[0].plan)
        except FactsToolError as exc:
            codes.append(exc.code)
        results.append(codes)
    world["capability_codes"] = results


@then("E_CAPABILITY is raised without fallback")
def capability_result(world):
    assert world["capability_codes"] == [["E_CAPABILITY"] * 6] * 2
