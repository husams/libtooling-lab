"""Newly discovered graph boundaries participate in the same recovery request."""
from pytest_bdd import given, then
from support.recovery import success, edge_names
from support.recovery_facts import component_tu


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
    run = context.recovery_run
    assert {("root", "bridge"), ("bridge", "deep"), ("deep", "leaf")} <= edge_names(run)
    assert not [row for row in run["recovery"] if row[1] == "failed"], run["recovery"]
    attempted = {row[0] for row in run["recovery"] if row[1] == "attempted"}
    library_tu = component_tu(context, "library")
    alt_tu = component_tu(context, "app", name="alternative.cpp")
    assert {library_tu, alt_tu} <= attempted, run["recovery"]
