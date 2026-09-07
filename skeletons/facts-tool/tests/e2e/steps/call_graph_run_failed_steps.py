"""Failure after traversal and forced persistence rollback examples (B-042)."""
import sqlite3
from pytest_bdd import parsers, then, when
from support.call_graph_matrix_db_cases import break_project_index, restore_project_index
from support.callgraph_run import (child_row_count, completion, edge_names, query,
                                   recovery, run_count, run_graph, run_row,
                                   stderr_lines)

CHILD_TABLES = ("callgraph_run_root", "callgraph_run_target", "callgraph_run_edge",
                "callgraph_run_frontier", "callgraph_run_recovery")
FORCED = "facts-tool: cannot persist call graph run: forced call graph edge failure"


def _children(facts):
    return [query(facts, f"SELECT COUNT(*) FROM {table}")[0][0] for table in CHILD_TABLES]


@when(parsers.parse("S-025 breaks the project index and recovers again at verbosity {verbosity:d}"))
def failed_after_traversal(context, verbosity):
    break_project_index(context)
    try:
        context.failed_result = run_graph(context, "--function", "root", "--recover-missing",
                                          verbosity=verbosity, env=context.recovery_env)
    finally:
        restore_project_index(context)
    context.failed_verbosity = verbosity


@then("the failed run exits 1 with a completion line of status failed")
def failed_exit(context):
    assert context.failed_result.returncode == 1, context.failed_result
    context.failed_run_id, status = completion(context.failed_result)
    assert status == "failed", status


@then("the failed run stderr carries exactly one non-verbose line naming the SQLite error")
def failed_stderr(context):
    lines = stderr_lines(context.failed_result)
    plain = [line for line in lines if not line.startswith(
        ("facts-tool: call-graph:", "facts-tool: roots selected",
         "facts-tool: graph traversal", "facts-tool: recovery-"))]
    assert len(plain) == 1 and "no such column: usr" in plain[0], lines
    assert (len(lines) == 1) == (context.failed_verbosity == 0), lines


@then("the failed run row has status failed with the SQLite error and its reached edges")
def failed_row(context):
    facts = context.facts_database_path
    row = run_row(facts, context.failed_run_id)
    assert row["status"] == "failed" and "no such column: usr" in row["error"], row
    assert row["truncation_reason"] is None, row
    assert {("root", "bridge"), ("bridge", "leaf")} == edge_names(facts, context.failed_run_id)
    assert recovery(facts, context.failed_run_id) == [], "a per-TU row was invented"


@when(parsers.parse("S-025 forces the edge insert to fail and runs again at verbosity {verbosity:d}"))
def forced_failure(context, verbosity):
    facts = context.facts_database_path
    context.forced_before = (run_count(facts), _children(facts))
    context.forced_prior = max(query(facts, "SELECT run_id FROM callgraph_run"))[0]
    context.forced_prior_edges = edge_names(facts, context.forced_prior)
    with sqlite3.connect(facts) as connection:
        connection.execute("CREATE TRIGGER b042_edge_failure BEFORE INSERT ON callgraph_run_edge "
                           "BEGIN SELECT RAISE(ABORT, 'forced call graph edge failure'); END")
    try:
        context.forced_result = run_graph(context, "--function", "root",
                                          verbosity=verbosity, env=context.recovery_env)
    finally:
        with sqlite3.connect(facts) as connection:
            connection.execute("DROP TRIGGER b042_edge_failure")
    context.forced_verbosity = verbosity


@then("the rolled-back run exits 1 with empty stdout and the forced SQLite diagnostic")
def forced_shape(context):
    result = context.forced_result
    assert result.returncode == 1 and result.stdout == "", result
    lines = stderr_lines(result)
    assert lines[-1] == FORCED and lines.count(FORCED) == 1, lines
    assert (len(lines) == 1) == (context.forced_verbosity == 0), lines


@then("no run or child rows were added and the earlier run is intact")
def forced_rollback(context):
    facts = context.facts_database_path
    assert (run_count(facts), _children(facts)) == context.forced_before
    assert run_row(facts, context.forced_prior)["status"] == "complete"
    assert edge_names(facts, context.forced_prior) == context.forced_prior_edges
    assert child_row_count(facts, context.forced_prior) > 0
