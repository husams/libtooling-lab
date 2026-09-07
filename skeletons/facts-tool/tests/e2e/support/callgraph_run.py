"""Read `analyse call-graph` runs back from the facts store (schema 12)."""
import re
import sqlite3
import subprocess

from support.callgraph_run_rows import (  # noqa: F401  re-exported helpers
    child_row_count, edge_names, edges, frontier, recovery, roots, target)

COMPLETION = re.compile(r"^facts-tool: call graph run (\d+) "
                        r"(complete|truncated|cancelled|recovery-failed|failed)$")
RUN_COLUMNS = ("run_id", "created_at", "project_path", "facts_path", "mode",
               "path_mode", "calls_scope", "components", "max_depth",
               "max_nodes", "max_edges", "time_limit_ms", "recover_missing",
               "status", "truncation_reason", "error")


def command(context, *args, conf=True, verbosity=0):
    argv = [str(context.facts_tool), "analyse", "call-graph", "-v",
            str(verbosity), "-f", str(context.facts_database_path)]
    if conf:
        argv += ["-c", str(context.files_database_path)]
    return argv + [str(value) for value in args]


def run_graph(context, *args, conf=True, verbosity=0, env=None):
    return subprocess.run(command(context, *args, conf=conf,
                                  verbosity=verbosity),
                          capture_output=True, text=True, check=False, env=env)


def completion(result):
    """The (run_id, status) named by the single stdout completion line."""
    lines = result.stdout.splitlines()
    assert len(lines) == 1, result.stdout + result.stderr
    match = COMPLETION.match(lines[0])
    assert match, result.stdout + result.stderr
    return int(match.group(1)), match.group(2)


def stderr_lines(result):
    return result.stderr.splitlines()


def query(facts, sql, parameters=()):
    with sqlite3.connect(facts) as connection:
        return connection.execute(sql, parameters).fetchall()


def run_count(facts):
    return query(facts, "SELECT COUNT(*) FROM callgraph_run")[0][0]


def run_ids(facts):
    return [row[0] for row in query(facts, "SELECT run_id FROM callgraph_run "
                                           "ORDER BY run_id")]


def latest_run_id(facts):
    return query(facts, "SELECT MAX(run_id) FROM callgraph_run")[0][0]


def run_row(facts, run_id):
    rows = query(facts, "SELECT " + ",".join(RUN_COLUMNS) +
                 " FROM callgraph_run WHERE run_id=?", (run_id,))
    assert len(rows) == 1, rows
    return dict(zip(RUN_COLUMNS, rows[0]))
