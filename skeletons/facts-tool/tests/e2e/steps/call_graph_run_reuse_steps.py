"""Read-only reuse and recovery-failure checks (migrated S-025)."""
from pytest_bdd import given, when, then, parsers
from support.recovery import success
from support.callgraph_run import run_graph, completion
from support.callgraph_run import recovery as recovery_rows
from support.database import file_snapshot, symbol_snapshot


@when("S-025 re-runs without recovery")
def rerun_read_only(context):
    context.graph_files_before = file_snapshot(context.files_database_path)
    context.graph_symbols_before = symbol_snapshot(context.facts_database_path)
    context.graph_bytes_before = context.files_database_path.read_bytes()
    context.graph_run_result = success(run_graph(
        context, "--function", "root", env=context.recovery_env))
    context.graph_run_id, context.graph_run_status = completion(context.graph_run_result)


@then("no new recovery rows were recorded")
def no_new_recovery(context):
    assert recovery_rows(context.facts_database_path, context.graph_run_id) == []


@then("no new symbols or definitions were extracted")
def no_new_symbols(context):
    assert symbol_snapshot(context.facts_database_path) == context.graph_symbols_before


@then("the project store bytes are unchanged")
def store_unchanged(context):
    assert context.files_database_path.read_bytes() == context.graph_bytes_before
    assert file_snapshot(context.files_database_path) == context.graph_files_before


@then("stderr is empty")
def stderr_empty(context):
    assert context.graph_run_result.stderr == "", context.graph_run_result.stderr


@given("the S-025 library fails compilation")
def broken_library(context):
    context.recovery_sources[1].write_text("#error S025_RECOVERY_FAILURE\n")


@then("the run exit code is 1")
def exit_one(context):
    assert context.graph_run_result.returncode == 1, context.graph_run_result


@then("the completion status is recovery-failed")
def status_recovery_failed(context):
    assert context.graph_run_status == "recovery-failed", context.graph_run_status


@then(parsers.parse('the library recovery row failed with a diagnostic containing "{text}"'))
def library_recovery_failed(context, text):
    rows = recovery_rows(context.facts_database_path, context.graph_run_id)
    failed = [row for row in rows if row[1] == "failed"]
    assert len(failed) == 1 and text in failed[0][2], failed
