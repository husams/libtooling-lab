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
    codes = []
    for query in queries:
        try:
            cb.executor.run(query.plan)
        except FactsToolError as exc:
            codes.append(exc.code)
    try:
        cb.executor.explain(queries[0].plan)
    except FactsToolError as exc:
        codes.append(exc.code)
    world["capability_codes"] = codes


@then("E_CAPABILITY is raised without fallback")
def capability_result(world):
    assert world["capability_codes"] == ["E_CAPABILITY"] * 6
