import json
import sqlite3

from pytest_bdd import then, when

from support.database import query, require
from support.entries import extract, import_project, lookup, run, succeed


@when("S-027 entry publication is forced to fail")
def force_failure(context):
    context.entry_before = query(context.facts_database_path,
                                "SELECT * FROM callgraph_entry ORDER BY symbol_id")
    with sqlite3.connect(context.facts_database_path) as connection:
        connection.execute("CREATE TRIGGER fail_entry BEFORE INSERT ON callgraph_entry "
                           "BEGIN SELECT RAISE(ABORT,'forced-entry-failure'); END")
    context.entry_failed = extract(context)


@then("committed S-027 graph entries survive the rollback and retry")
def rollback(context):
    require(context.entry_failed.returncode == 1,
            context.entry_failed.stdout + context.entry_failed.stderr)
    require(query(context.facts_database_path,
                  "SELECT * FROM callgraph_entry ORDER BY symbol_id") == context.entry_before,
            "entry rollback lost prior committed state")
    with sqlite3.connect(context.facts_database_path) as connection:
        connection.execute("DROP TRIGGER fail_entry")
    succeed(extract(context))
    require(lookup(context)["entry_available"], "retry did not publish")


@when("the S-027 source command is changed and reimported")
def reimport(context):
    path = context.entry_root / "compile_commands.json"
    commands = json.loads(path.read_text())
    commands[0]["arguments"].insert(1, "-DS027_CHANGED=1")
    path.write_text(json.dumps(commands))
    import_project(context)


@then("failed S-027 invalidation preserves the prior project commands")
def invalidation_failure(context):
    before = query(context.files_database_path,
                   "SELECT id,compile_options FROM file ORDER BY id")
    with sqlite3.connect(context.facts_database_path) as connection:
        connection.execute("CREATE TRIGGER fail_invalidation BEFORE DELETE ON callgraph_entry "
                           "BEGIN SELECT RAISE(ABORT,'forced-entry-invalidation'); END")
    path = context.entry_root / "compile_commands.json"
    commands = json.loads(path.read_text())
    commands[0]["arguments"].insert(1, "-DS027_REFUSE=1")
    path.write_text(json.dumps(commands))
    result = run(context, "import", "-v", "0", "--conf", context.files_database_path,
                 "--facts", context.facts_database_path, "-p", context.entry_root)
    require(result.returncode != 0 and "invalidat" in result.stderr.lower(),
            result.stdout + result.stderr)
    require(query(context.files_database_path,
                  "SELECT id,compile_options FROM file ORDER BY id") == before,
            "project commands committed despite invalidation failure")


@then("S-027 extraction leaves the guarded matched symbol index unchanged")
def no_index_writes(context):
    with sqlite3.connect(context.files_database_path) as connection:
        for action in ("INSERT", "UPDATE", "DELETE"):
            connection.execute(f"CREATE TRIGGER entry_guard_{action} BEFORE {action} "
                               "ON matched_symbol_index BEGIN "
                               "SELECT RAISE(ABORT,'index-write-during-extract'); END")
    succeed(extract(context))
    require(query(context.files_database_path, "SELECT * FROM matched_symbol_index") == [],
            "extract populated index")
    require(query(context.files_database_path,
                  "SELECT name FROM pragma_table_info('matched_symbol_index')") ==
            [("usr",), ("qualified_name",), ("file_id",), ("kind",)], "index schema grew")
