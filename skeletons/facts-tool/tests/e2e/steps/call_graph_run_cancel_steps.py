"""SIGINT-synchronised deterministic examples: recovery and pre-traversal (B-042)."""
import signal
import subprocess
import threading
from pytest_bdd import when, then
from support.callgraph_run import run_count, run_row, frontier, completion
from support.callgraph_run import edges as run_edges
from support.call_graph_large_chain import build as build_large_chain


@when("S-025 grows the library and interrupts recovery at the validation checkpoint")
def interrupt_recovery(context):
    source = context.recovery_sources[1]
    source.write_text(source.read_text() + "\n".join(
        f"struct S025CancelType{i} {{ int field; }};" for i in range(15000)))
    args = [str(context.facts_tool), "analyse", "call-graph", "-v", "3",
            "-c", str(context.files_database_path), "-f", str(context.facts_database_path),
            "--function", "root", "--recover-missing"]
    captured = []
    with subprocess.Popen(args, env=context.recovery_env, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True) as process:
        def drain():
            for line in process.stderr:
                captured.append(line)
                if "recovery validation tu=" in line:
                    process.send_signal(signal.SIGINT)
        reader = threading.Thread(target=drain)
        reader.start()
        context.cancel_returncode = process.wait(timeout=90)
        reader.join(timeout=10)
        context.cancel_stdout = process.stdout.read()
    context.cancel_stderr = "".join(captured)


@then("the interrupted run exits 130 with the cancellation completion line")
def interrupted_exit(context):
    assert context.cancel_returncode == 130, context.cancel_stderr
    result = subprocess.CompletedProcess([], 130, context.cancel_stdout, context.cancel_stderr)
    context.cancel_run_id, status = completion(result)
    assert status == "cancelled", status


@then("the interrupted run status is cancelled with reason cancelled")
def interrupted_status(context):
    row = run_row(context.facts_database_path, context.cancel_run_id)
    assert row["status"] == "cancelled" and row["truncation_reason"] == "cancelled", row


@then("the interrupted run has a cancelled frontier row and edge root->bridge")
def interrupted_shape(context):
    reasons = [reason for _, reason in
              frontier(context.facts_database_path, context.cancel_run_id)]
    found = {(e["source"], e["target"]) for e in
             run_edges(context.facts_database_path, context.cancel_run_id)}
    assert "cancelled" in reasons, reasons
    assert ("root", "bridge") in found, found


@when("S-025 interrupts a large all-roots traversal before it starts")
def interrupt_before_traversal(context):
    _files_db, facts_db = build_large_chain(context, count=5000)
    context.large_facts_db = facts_db
    context.large_run_before = run_count(facts_db)
    args = [str(context.facts_tool), "analyse", "call-graph", "-v", "2",
            "-f", str(facts_db), "--all"]
    captured = []
    with subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          text=True) as process:
        def drain():
            for line in process.stderr:
                captured.append(line)
                if "roots selected" in line:
                    process.send_signal(signal.SIGINT)
        reader = threading.Thread(target=drain)
        reader.start()
        context.cancel_returncode = process.wait(timeout=90)
        reader.join(timeout=10)
        context.cancel_stdout = process.stdout.read()
    context.cancel_stderr = "".join(captured)


@then("the interrupt lands with exit 130 and empty stdout")
def interrupt_shape(context):
    assert context.cancel_returncode == 130, context.cancel_stderr
    assert context.cancel_stdout == "", context.cancel_stdout


@then('the last stderr line is "facts-tool: cancelled before traversal"')
def last_stderr_line(context):
    lines = [line for line in context.cancel_stderr.splitlines() if line]
    assert lines[-1] == "facts-tool: cancelled before traversal", lines[-3:]


@then("no call graph run was recorded for the large corpus")
def no_large_run(context):
    assert run_count(context.large_facts_db) == context.large_run_before
