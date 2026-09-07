"""Two deterministic examples: dual-run ordering and read-only commit failure."""
from pytest_bdd import when, then
from support.recovery import success
from support.callgraph_run import run_graph, completion, run_row, run_count, query
from support.callgraph_run import edges as run_edges

CHILD_TABLES = ("callgraph_run_root", "callgraph_run_target", "callgraph_run_edge",
               "callgraph_run_frontier", "callgraph_run_recovery")


def _total_child_rows(facts):
    return sum(query(facts, f"SELECT COUNT(*) FROM {table}")[0][0]
              for table in CHILD_TABLES)


@when("S-025 runs a project-scoped depth-1 query for root")
def dual_run_second(context):
    context.graph_run_a = context.graph_run_id
    result = success(run_graph(
        context, "--function", "root", "--calls-scope", "project",
        "--max-depth", "1", env=context.recovery_env))
    context.graph_run_b, _ = completion(result)


@then("run B has a greater run_id than run A")
def run_ordering(context):
    assert context.graph_run_b > context.graph_run_a, (context.graph_run_a, context.graph_run_b)


@then("run A has both edges and status complete")
def run_a_shape(context):
    row = run_row(context.facts_database_path, context.graph_run_a)
    found = {(e["source"], e["target"]) for e in
             run_edges(context.facts_database_path, context.graph_run_a)}
    assert row["status"] == "complete", row
    assert found == {("root", "bridge"), ("bridge", "leaf")}, found


@then("run B has only root->bridge and status truncated with reason max_depth")
def run_b_shape(context):
    row = run_row(context.facts_database_path, context.graph_run_b)
    found = {(e["source"], e["target"]) for e in
             run_edges(context.facts_database_path, context.graph_run_b)}
    assert row["status"] == "truncated" and row["truncation_reason"] == "max_depth", row
    assert found == {("root", "bridge")}, found


@when("S-025 makes the facts store read-only and runs again")
def commit_failure(context):
    context.commit_before = run_count(context.facts_database_path)
    context.commit_children_before = _total_child_rows(context.facts_database_path)
    context.facts_database_path.chmod(0o444)
    context.graph_run_result = run_graph(
        context, "--function", "root", env=context.recovery_env)
    context.facts_database_path.chmod(0o644)


@then("the commit failure exits 1 with empty stdout")
def commit_exit(context):
    assert context.graph_run_result.returncode == 1
    assert context.graph_run_result.stdout == ""


@then("the commit failure stderr is exactly the readonly diagnostic")
def commit_stderr(context):
    assert context.graph_run_result.stderr == (
        "facts-tool: cannot persist call graph run: "
        "attempt to write a readonly database\n")


@then("no run or child rows were added by the failed commit")
def commit_no_rows(context):
    assert run_count(context.facts_database_path) == context.commit_before
    assert _total_child_rows(context.facts_database_path) == context.commit_children_before
