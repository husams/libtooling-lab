from __future__ import annotations

import sqlite3

from pytest_bdd import then, when

from steps.matched_symbol_index_steps import find, match_caller
from steps.native_matcher_workflow_steps import run
from support.database import query, require
from support.scenario import FactsToolContext


def guard(connection: sqlite3.Connection) -> None:
    for action in ("INSERT", "UPDATE", "DELETE"):
        connection.execute(f"CREATE TRIGGER guard_{action.lower()} BEFORE {action} ON matched_symbol_index "
                           "BEGIN SELECT RAISE(ABORT,'matched-index-write'); END")


@when("extraction runs against empty and populated facts under an index write guard")
def guarded_extraction(context: FactsToolContext) -> None:
    context.s026_before = find(context, "--name", "targeted_match")["matches"]
    with sqlite3.connect(context.files_database) as connection:
        guard(connection)
    context.s026_extracts = []
    for output in (context.run_root_path / "s026-empty.sqlite", context.facts_database):
        result = run([str(context.facts_tool), "extract", "-v", "0", "--conf",
                      str(context.files_database), "--output", str(output),
                      str(context.targeted_match_source)])
        context.s026_extracts.append(result)


@then("extraction succeeds without changing matched candidates")
def extraction_unchanged(context: FactsToolContext) -> None:
    require(all(result.returncode == 0 for result in context.s026_extracts),
            "\n".join(result.stdout + result.stderr for result in context.s026_extracts))
    require(find(context, "--name", "targeted_match")["matches"] == context.s026_before,
            "extraction changed matched candidates")


@when("matched-index publication is forced to fail")
def fail_publication(context: FactsToolContext) -> None:
    with sqlite3.connect(context.files_database) as connection:
        connection.execute("CREATE TRIGGER fail_index BEFORE INSERT ON matched_symbol_index "
                           "BEGIN SELECT RAISE(ABORT,'forced-index-failure'); END")
    context.s026_failed = match_caller(context)


@then("facts are committed without a false index row and retry succeeds")
def failure_ordering(context: FactsToolContext) -> None:
    output = context.s026_failed.stdout + context.s026_failed.stderr
    require(context.s026_failed.returncode == 1 and "facts_committed:true,index_committed:false" in output, output)
    require(query(context.facts_database, "SELECT qualified_name FROM symbol WHERE qualified_name='targeted_match::caller'"), "facts rolled back")
    require(find(context, "--name", "targeted_match::caller")["matches"] == [], "false index row")
    with sqlite3.connect(context.files_database) as connection:
        connection.execute("DROP TRIGGER fail_index")
    retried = match_caller(context)
    require(retried.returncode == 0, retried.stdout + retried.stderr)
    require(len(find(context, "--name", "targeted_match::caller")["matches"]) == 1, "retry did not index")


@when("combined-store matched-index publication is forced to fail")
def fail_combined_publication(context: FactsToolContext) -> None:
    with sqlite3.connect(context.files_database) as connection:
        connection.execute("CREATE TRIGGER fail_combined BEFORE INSERT ON matched_symbol_index "
                           "BEGIN SELECT RAISE(ABORT,'forced-index-failure'); END")
    context.s026_combined = run([str(context.facts_tool), "match", "-v", "0", "--facts",
                                 str(context.files_database), "--matcher",
                                 'functionDecl(hasName("targeted_match::caller")).bind("symbol")',
                                 str(context.targeted_match_source)])


@then("combined-store facts and index rows are both rolled back")
def combined_rollback(context: FactsToolContext) -> None:
    output = context.s026_combined.stdout + context.s026_combined.stderr
    require(context.s026_combined.returncode == 1 and
            "facts_committed:false,index_committed:false" in output, output)
    require(query(context.files_database, "SELECT count(*) FROM matched_symbol_index") == [(0,)], "index committed")
    require(query(context.files_database, "SELECT count(*) FROM symbol") == [(0,)], "facts committed")


@when("its matched index file is cleared then rematched and removed")
def clear_rematch_remove(context: FactsToolContext) -> None:
    match = find(context, "--name", "targeted_match::caller")["matches"][0]
    clear = run([str(context.facts_tool), "symbol", "index", "clear", "-v", "0", "--conf",
                 str(context.files_database), "--file-id", str(match["file_id"])])
    require(clear.returncode == 0 and find(context, "--name", "targeted_match")["matches"] == [], clear.stdout + clear.stderr)
    require(match_caller(context).returncode == 0, "rematch failed")
    removed = run([str(context.facts_tool), "file", "remove", "-v", "0", "--conf",
                   str(context.files_database), str(context.targeted_match_source)])
    require(removed.returncode == 0, removed.stdout + removed.stderr)


@then("no matched candidate remains for that file")
def removed_candidate(context: FactsToolContext) -> None:
    require(find(context, "--name", "targeted_match")["matches"] == [], "cascade failed")
