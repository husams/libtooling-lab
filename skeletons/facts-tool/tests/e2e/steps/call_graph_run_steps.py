"""Background and the core migrated S-025 persistence checks."""
from pytest_bdd import given, when, then, parsers
from support.recovery import prepare
from support.callgraph_run import run_graph, completion, run_row
from support.callgraph_run import edges, roots


@given("the S-025 two-component graph fixture")
def prepare_fixture(context):
    prepare(context)


def _recover_root(context):
    context.graph_run_result = run_graph(
        context, "--function", "root", "--recover-missing", env=context.recovery_env)
    context.graph_run_id, context.graph_run_status = completion(context.graph_run_result)


@given("S-025 recovers the run for function root")
def recover_root_given(context):
    _recover_root(context)


@when("S-025 recovers the run for function root")
def recover_root_when(context):
    _recover_root(context)


@then("the run status is complete")
def status_complete(context):
    assert context.graph_run_status == "complete", context.graph_run_status


@then("the run status is truncated")
def status_truncated(context):
    assert context.graph_run_status == "truncated", context.graph_run_status


@then(parsers.parse("the run edges are {a}->{b} and {c}->{d}"))
def edges_two(context, a, b, c, d):
    found = {(e["source"], e["target"]) for e in
             edges(context.facts_database_path, context.graph_run_id)}
    assert found == {(a, b), (c, d)}, found


@then(parsers.parse("the run edges are only {a}->{b}"))
def edges_one(context, a, b):
    found = {(e["source"], e["target"]) for e in
             edges(context.facts_database_path, context.graph_run_id)}
    assert found == {(a, b)}, found


@then("the run row paths match the resolved project and facts databases")
def row_paths(context):
    row = run_row(context.facts_database_path, context.graph_run_id)
    assert row["project_path"] == str(context.files_database_path.resolve())
    assert row["facts_path"] == str(context.facts_database_path.resolve())


@then(parsers.parse("the run recovered missing definitions with roots [{name}]"))
def recovered_roots(context, name):
    row = run_row(context.facts_database_path, context.graph_run_id)
    assert row["recover_missing"] == 1, row
    names = [n for n, _ in roots(context.facts_database_path, context.graph_run_id)]
    assert names == [name], names


@then("stderr has exactly one line")
def one_stderr_line(context):
    lines = context.graph_run_result.stderr.splitlines()
    assert len(lines) == 1, lines
