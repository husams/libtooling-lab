"""Recovery must not create persistent attempt or failure state."""
import sqlite3
from pytest_bdd import given, then


def schemas(context):
    """sqlite_master snapshot, excluding the contracted callgraph_run* tables."""
    result = []
    for path in (context.files_database_path, context.facts_database_path):
        with sqlite3.connect(path) as db:
            result.append(db.execute(
                "SELECT type,name,sql FROM sqlite_master WHERE name NOT LIKE "
                "'callgraph_run%' ORDER BY type,name").fetchall())
    return result


@given("S-021 persistent schemas are recorded")
def snapshot(context):
    context.recovery_schemas = schemas(context)


@given("the S-021 registered library input is missing")
def missing_input(context):
    context.recovery_sources[1].unlink()


@then("S-021 adds no persisted failure or attempt schema")
def unchanged(context):
    assert schemas(context) == context.recovery_schemas
