import json
import shutil
import sqlite3

from pytest_bdd import parsers, then

from support.database import query, require
from support.entries import extract, lookup, run, succeed


def configuration(context):
    path = context.entry_root / "defaults.yaml"
    path.write_text("facts_template: " + json.dumps(str(context.facts_database_path)) + "\n")
    return ["--conf", context.files_database_path, "--config", path, "-v", "0"]


@then("S-027 catalog reads and dry runs preserve entries")
def readonly(context):
    before = context.facts_database_path.read_bytes()
    succeed(run(context, "file", "list", *configuration(context)))
    succeed(run(context, "component", "rm", "--name", "app", "--dry-run",
                *configuration(context)))
    require(context.facts_database_path.read_bytes() == before,
            "read or dry run invalidated entries")
    require(lookup(context)["entry_available"], "read removed entry")


@then("S-027 catalog option edits invalidate configured entries")
def options(context):
    succeed(run(context, "file", "set-option", "--match", "app.cpp$",
                "--arg=-DS027_CATALOG=1", *configuration(context)))
    require(not lookup(context)["entry_available"], "catalog edit retained entry")
    succeed(extract(context))
    require(lookup(context)["entry_available"], "catalog edit prevented regeneration")


@then("S-027 import without explicit facts invalidates configured entries")
def imported(context):
    succeed(run(context, "import", "-p", context.entry_root,
                *configuration(context)))
    require(not lookup(context)["entry_available"], "configured import retained entry")


@then("S-027 failed catalog invalidation leaves project options unchanged")
def failed(context):
    before = query(context.files_database_path,
                   "SELECT id,compile_options FROM file ORDER BY id")
    with sqlite3.connect(context.facts_database_path) as connection:
        connection.execute("CREATE TRIGGER refuse_catalog BEFORE DELETE ON callgraph_entry "
                           "BEGIN SELECT RAISE(ABORT,'forced-catalog-invalidation'); END")
    result = run(context, "file", "set-option", "--match", "app.cpp$",
                 "--arg=-DS027_REJECT=1", *configuration(context))
    require(result.returncode != 0, result.stdout + result.stderr)
    require(query(context.files_database_path,
                  "SELECT id,compile_options FROM file ORDER BY id") == before,
            "catalog committed after failed invalidation")


@then(parsers.parse('S-027 "{operation}" invalidates all source-based configured stores'))
def source_templates(context, operation):
    destinations = [context.entry_root / f"{source.stem}.db"
                    for source in context.entry_sources]
    for path in destinations:
        shutil.copyfile(context.facts_database_path, path)
    yaml = context.entry_root / "source-defaults.yaml"
    template = str(context.entry_root / "{filename}.db")
    yaml.write_text("facts_template: " + json.dumps(template) + "\n")
    arguments = ["--conf", context.files_database_path, "--config", yaml, "-v", "0"]
    command = (["import", "-p", context.entry_root] if operation == "import" else
               ["file", "set-option", "--match", "app.cpp$", "--arg=-DS027_ALL=1"])
    succeed(run(context, *command, *arguments))
    for path in destinations:
        require(not lookup(context, facts=path)["entry_available"],
                f"{operation} retained entries in {path}")
