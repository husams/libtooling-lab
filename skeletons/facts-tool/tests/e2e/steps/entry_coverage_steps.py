import sqlite3

from pytest_bdd import then, when

from support.entries import graph, lookup
from support.database import require


@when("S-027 catalog metadata marks the entry source complete")
def mark_complete(context):
    with sqlite3.connect(context.files_database_path) as database:
        database.execute("UPDATE file SET indexed=1,indexed_at='2026-09-06T00:00:00Z',mtime=NULL "
                         "WHERE name='app.cpp'")


@then("S-027 entry lookup reports validated complete coverage")
def complete_coverage(context):
    entry = lookup(context, "left")
    require(entry["pair"]["state"] == "validated" and
            entry["definition_availability"] == "available" and
            entry["extraction_coverage"]["state"] == "complete" and
            entry["coverage"]["state"] == "complete" and
            entry["coverage"]["freshness"] == "fresh" and
            entry["coverage"]["catalog_indexed"] is True, str(entry))


@then("S-027 entry aggregate coverage reports the missing library definition")
def aggregate_missing_definition(context):
    entry = lookup(context, "boundary")
    regular = graph(context, "boundary")
    require(entry["coverage"]["state"] == "complete" and
            entry["definition_availability"] == "available" and
            entry["extraction_coverage"]["state"] == "incomplete" and
            regular["extraction_coverage"]["state"] ==
            entry["extraction_coverage"]["state"], str(entry))


@when("S-027 catalog metadata marks the entry source stale")
def mark_stale(context):
    with sqlite3.connect(context.files_database_path) as database:
        database.execute("UPDATE file SET indexed=1,indexed_at='2026-09-06T00:00:00Z',mtime=0 "
                         "WHERE name='app.cpp'")


@then("S-027 entry lookup reports stale coverage with a refresh action")
def stale_coverage(context):
    entry = lookup(context, "left")
    require(entry["pair"]["state"] == "validated" and
            entry["extraction_coverage"]["state"] == "stale" and
            entry["coverage"]["state"] == "stale" and
            entry["coverage"]["freshness"] == "stale" and
            entry["coverage"]["action"] == "refresh-source", str(entry))


@then("S-027 entry lookup without project configuration keeps coverage unknown")
def unknown_coverage_without_project(context):
    entry = lookup(context, "left", conf=False)
    require(entry["pair"]["state"] == "unavailable" and
            entry["coverage"]["state"] == "unknown" and
            entry["coverage"]["freshness"] == "unknown" and
            entry["extraction_coverage"]["state"] == "unknown", str(entry))
