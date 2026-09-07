"""Recovery behavior exercised exclusively through the native command."""
from pytest_bdd import given, when, then
from support.callgraph_run import run_count
from support.recovery import prepare, graph, edge_names, extract, seed_match, success
from support.recovery_facts import component_tu, component_arguments, facts_snapshot


@given("an S-021 app calls an unextracted registered library")
def fixture(context):
    prepare(context)


@given("the S-021 library has only symbol-match evidence")
def symbol_only(context):
    seed_match(context)


@given("the S-021 library already has valid call facts")
def complete(context):
    success(extract(context, 1))


@when("S-021 missing recovery is requested")
def recover(context):
    context.recovery_result, context.recovery_run = graph(context, verbosity=1)


@then("S-021 recovers the library body with its stored command")
def recovered(context):
    result, run = context.recovery_result, context.recovery_run
    success(result)
    assert {("root", "bridge"), ("bridge", "leaf")} <= edge_names(run), run
    assert not [row for row in run["recovery"] if row[1] == "failed"], run["recovery"]
    library_tu = component_tu(context, "library")
    attempted = [row for row in run["recovery"] if row[1] == "attempted"]
    assert any(row[0] == library_tu for row in attempted), run["recovery"]
    assert "-DS021_LIBRARY=1" in component_arguments(context, "library")
    assert "recovery-start" in result.stderr and "recovery-complete" in result.stderr


@then("S-021 reuses existing facts without extraction")
def reused(context):
    success(context.recovery_result)
    run = context.recovery_run
    assert not [row for row in run["recovery"] if row[1] == "attempted"], run["recovery"]
    assert any(row[1] == "reused" for row in run["recovery"]), run["recovery"]
    assert ("bridge", "leaf") in edge_names(run)


@then("omitting S-021 recovery preserves the partial graph and stores")
def readonly(context):
    facts = context.facts_database_path
    project_before = context.files_database_path.read_bytes()
    facts_before = facts_snapshot(facts)
    count_before = run_count(facts)
    result, run = graph(context, recover=False)
    success(result)
    assert ("root", "bridge") in edge_names(run)
    assert ("bridge", "leaf") not in edge_names(run)
    assert context.files_database_path.read_bytes() == project_before
    assert facts_snapshot(facts) == facts_before
    assert run_count(facts) == count_before + 1
    assert run["recovery"] == []
    assert "recovery-start" not in result.stderr
