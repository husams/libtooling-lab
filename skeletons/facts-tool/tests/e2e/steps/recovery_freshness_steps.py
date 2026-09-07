"""Unknown production freshness requires explicit current-body validation."""
from pytest_bdd import given, then
from support.recovery import extract, success, edge_names
from support.recovery_facts import component_tu


@given("the S-021 previously extracted library gains another call")
def changed_body(context):
    success(extract(context, 1))
    context.recovery_sources[1].write_text(
        context.recovery_library_body.replace(
            "int bridge()", "int changed() { return 1; } int bridge()"
        ).replace("return leaf();", "return leaf() + changed();"))


@then("S-021 refreshes the changed library call evidence")
def refreshed(context):
    success(context.recovery_result)
    run = context.recovery_run
    assert ("bridge", "changed") in edge_names(run)
    library_tu = component_tu(context, "library")
    assert any(row[0] == library_tu and row[1] == "attempted"
               for row in run["recovery"]), run["recovery"]
