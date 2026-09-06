"""Real failed compiler inputs and an index-write storage guard."""
import sqlite3
from pytest_bdd import given, when, then
from support.recovery import graph, edge_names


@given("the S-021 registered library has a syntax error")
def broken(context):
    context.recovery_sources[1].write_text("int bridge( { syntax error\n")


@then("S-021 reports the failed library once with the partial graph")
def failure(context):
    result, data = context.recovery_result, context.recovery_graph
    assert result.returncode == 1, result.stdout + result.stderr
    assert ("root", "bridge") in edge_names(data), data
    assert "recovery-failed" in str(data["errors"]), data
    assert data["coverage"]["missing_definitions"] or data["coverage"]["unresolved_targets"], data
    failed = data["recovery"]["failed"]
    library = [entry for entry in failed if entry["component"] == "library"]
    assert len(library) == 1 and library[0]["reason"], failed
    attempts = data["recovery"]["attempted"]
    keys = [(entry["tu_file_id"], tuple(entry["arguments"])) for entry in attempts]
    assert len(keys) == len(set(keys)), attempts
    assert "recovery-complete" in result.stderr


@when("the S-021 library is repaired and recovery is requested again")
def repair(context):
    context.recovery_sources[1].write_text(context.recovery_library_body)
    context.recovery_result, context.recovery_graph = graph(context)


def index_rows(context):
    with sqlite3.connect(context.files_database_path) as db:
        return db.execute("SELECT * FROM matched_symbol_index ORDER BY usr,file_id").fetchall()


@given("S-021 match-index writes are guarded")
def guard(context):
    context.recovery_index_before = index_rows(context)
    assert context.recovery_index_before
    with sqlite3.connect(context.files_database_path) as db:
        for operation in ("INSERT", "UPDATE", "DELETE"):
            db.execute(f"CREATE TRIGGER s021_guard_{operation} BEFORE {operation} "
                       "ON matched_symbol_index BEGIN SELECT RAISE(ABORT, "
                       "'recovery extraction wrote the match-only index'); END")


@then("S-021 match-index contents remain unchanged")
def index_unchanged(context):
    assert index_rows(context) == context.recovery_index_before
