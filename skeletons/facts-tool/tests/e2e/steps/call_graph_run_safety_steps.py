"""Invalid call-graph requests write no run and start no recovery (B-042).

The "configuration" kind is verified against the real binary at exit 1 with
message "project configuration database not found: ...", not the exit 3
"configuration error:" family CONTRACT.md's outcome matrix documents for a
missing --conf database -- see implementation-notes.md for the discrepancy.
"""
import subprocess
from pytest_bdd import when, then, parsers
from support.callgraph_run import command, run_count

KINDS = {
    "root": ("--function", "missing-root"),
    "target": ("--function", "root", "--to", "missing-target"),
    "component": ("--function", "root", "--component", "missing-component"),
}


@when(parsers.parse("S-025 requests an invalid {kind} call graph run"))
def invalid_request(context, kind):
    context.invalid_before = run_count(context.facts_database_path)
    if kind == "configuration":
        argv = [str(context.facts_tool), "analyse", "call-graph", "-v", "0",
                "-f", str(context.facts_database_path), "-c",
                str(context.run_root_path / "missing.db"), "--function", "root"]
    else:
        argv = command(context, *KINDS[kind])
    context.graph_run_result = subprocess.run(
        argv, capture_output=True, text=True, env=context.recovery_env)


@then(parsers.parse("the invalid request exit code is {code:d}"))
def invalid_exit(context, code):
    assert context.graph_run_result.returncode == code, context.graph_run_result


@then("no recovery-start was printed")
def no_recovery_start(context):
    assert "recovery-start" not in context.graph_run_result.stderr


@then("no run was written")
def no_run_written(context):
    assert context.graph_run_result.stdout == ""
    assert run_count(context.facts_database_path) == context.invalid_before
