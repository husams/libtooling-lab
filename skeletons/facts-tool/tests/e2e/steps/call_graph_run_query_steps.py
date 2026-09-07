"""Repeated recovery, configured defaults, budgets, and query modes."""
import subprocess
from pytest_bdd import when, then, parsers
from support.recovery import success
from support.callgraph_run import run_graph, completion, run_row
from support.callgraph_run import frontier, edges, recovery as recovery_rows


@when(parsers.parse("S-025 repeats recovery at verbosity {level:d}"))
def repeat_recovery(context, level):
    context.graph_run_result = success(run_graph(
        context, "--function", "root", "--recover-missing",
        verbosity=level, env=context.recovery_env))
    context.graph_run_id, _ = completion(context.graph_run_result)


@then(parsers.parse('stderr has exactly one "{text}" line'))
def one_matching_line(context, text):
    lines = [line for line in context.graph_run_result.stderr.splitlines()
             if line == text]
    assert len(lines) == 1, context.graph_run_result.stderr


@then("no recovery rows were attempted")
def no_attempted(context):
    attempted = [row for row in recovery_rows(context.facts_database_path,
                                              context.graph_run_id)
                if row[1] == "attempted"]
    assert attempted == [], attempted


@when("S-025 renders using configured pair defaults")
def configured_defaults(context):
    config = context.run_root_path / "graph-config.yaml"
    config.write_text(f"conf_template: '{context.files_database_path}'\n"
                      f"facts_template: '{context.facts_database_path}'\n")
    argv = [str(context.facts_tool), "analyse", "call-graph", "-v", "0",
            "--function", "root", "--recover-missing", "--config", str(config)]
    context.graph_run_result = success(subprocess.run(
        argv, capture_output=True, text=True, env=context.recovery_env))
    context.graph_run_id, context.graph_run_status = completion(context.graph_run_result)


@when("S-025 runs with a one-node budget")
def one_node_budget(context):
    context.graph_run_result = success(run_graph(
        context, "--function", "root", "--max-nodes", "1", env=context.recovery_env))
    context.graph_run_id, context.graph_run_status = completion(context.graph_run_result)


@then(parsers.parse("the truncation reason is {reason}"))
def truncation_reason(context, reason):
    row = run_row(context.facts_database_path, context.graph_run_id)
    assert row["truncation_reason"] == reason, row


@then("the frontier is not empty")
def frontier_not_empty(context):
    assert frontier(context.facts_database_path, context.graph_run_id) != []


@then("exactly one distinct node was reached")
def one_node_reached(context):
    nodes = {node for edge in edges(context.facts_database_path, context.graph_run_id)
            for node in (edge["source"], edge["target"])}
    assert len(nodes) <= 1, nodes


@when(parsers.parse("S-025 runs a {mode:w} query"))
def query_mode(context, mode):
    args = ("--function", "leaf", "--direction", "callers") if mode == "callers" \
        else ("--function", "root", "--to", "leaf")
    context.graph_run_result = success(run_graph(context, *args, env=context.recovery_env))
    context.graph_run_id, context.graph_run_status = completion(context.graph_run_result)


@then(parsers.parse("the run row mode is {mode}"))
def row_mode(context, mode):
    row = run_row(context.facts_database_path, context.graph_run_id)
    assert row["mode"] == mode, row


@when(parsers.parse("S-025 runs a budgeted {mode:w} query"))
def budgeted_query(context, mode):
    args = ("--function", "leaf", "--direction", "callers", "--max-nodes", "1") \
        if mode == "callers" else ("--function", "root", "--to", "leaf", "--max-depth", "1")
    context.graph_run_result = success(run_graph(context, *args, env=context.recovery_env))
    context.graph_run_id, context.graph_run_status = completion(context.graph_run_result)
