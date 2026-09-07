"""One targeted Calls match must not certify a function's complete body."""
from pytest_bdd import given, then
from support.recovery import run, success, edge_names, graph


@given("S-021 has matched only one of two calls in the library body")
def partial(context):
    source = context.recovery_sources[1]
    source.write_text("int leaf() { return 1; } int second_leaf() { return 2; }\n"
                      "int bridge() { return leaf() + second_leaf(); }\n")
    success(run(context, "match", "-v", "0", "--conf", context.files_database_path,
                "--facts", context.facts_database_path, "--matcher",
                'callExpr(callee(functionDecl(hasName("leaf")).bind("callee"))).bind("call")',
                source))
    result, data = graph(context, recover=False)
    success(result)
    assert ("bridge", "leaf") in edge_names(data), data
    assert ("bridge", "second_leaf") not in edge_names(data), data


@then("S-021 recovers the missing second call despite the existing first call")
def missing_call(context):
    success(context.recovery_result)
    assert {("bridge", "leaf"), ("bridge", "second_leaf")} <= edge_names(context.recovery_graph)
