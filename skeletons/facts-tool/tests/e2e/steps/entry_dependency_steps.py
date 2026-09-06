import sqlite3

from pytest_bdd import then

from support.database import query, require
from support.entries import extract, lookup, run, succeed


def dependency(context):
    return run(context, "analyse", "dependency", "-v", "0", "--conf",
               context.files_database_path, "--output", context.facts_database_path,
               context.entry_sources[0])


@then("S-027 dependency writes invalidate entries and failures roll back")
def lifecycle(context):
    succeed(dependency(context))
    require(not lookup(context)["entry_available"], "dependency write retained entry")
    succeed(extract(context))
    before = query(context.facts_database_path,
                   "SELECT * FROM callgraph_entry ORDER BY symbol_id")
    with sqlite3.connect(context.facts_database_path) as connection:
        connection.execute("CREATE TRIGGER refuse_dependency BEFORE INSERT ON include_dependency "
                           "BEGIN SELECT RAISE(ABORT,'forced-dependency-failure'); END")
    failed = dependency(context)
    require(failed.returncode != 0, failed.stdout + failed.stderr)
    require(query(context.facts_database_path,
                  "SELECT * FROM callgraph_entry ORDER BY symbol_id") == before,
            "failed dependency write did not restore entries")
