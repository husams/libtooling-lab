"""Newly discovered graph boundaries participate in the same recovery request."""
from pytest_bdd import given, then
from support.recovery import success, edge_names


@given("recovering S-021 bridge reveals a missing definition in another TU")
def chained(context):
    context.recovery_sources[1].write_text(
        "int deep();\n" + context.recovery_library_body.replace(
            "int bridge() { return leaf(); }", "int bridge() { return deep(); }"))
    context.recovery_alternative.write_text(
        "int leaf(); int deep() { return leaf(); }\n")


@then("S-021 follows the newly discovered boundary until stored evidence is usable")
def recovered_chain(context):
    success(context.recovery_result)
    data = context.recovery_graph
    assert {("root", "bridge"), ("bridge", "deep"), ("deep", "leaf")} <= edge_names(data)
    assert not data["recovery"]["failed"], data
    assert {"app", "library"} <= {item["component"] for item in data["recovery"]["attempted"]}
