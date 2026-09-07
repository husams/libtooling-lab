from __future__ import annotations

import json
import sqlite3

from pytest_bdd import given, then, when

from steps.matched_symbol_index_steps import find
from steps.native_matcher_workflow_steps import run
from support.database import require
from support.scenario import FactsToolContext


@given("an S-026 duplicate identity project")
def duplicate_project(context: FactsToolContext) -> None:
    context.prepare()
    context.s026_sources = []
    for number in range(2):
        source = context.run_root_path / f"duplicate-{number}.cpp"
        source.write_text("void shared(); namespace { void duplicate() {} }\n", encoding="utf-8")
        context.s026_sources.append(source)
    context._write_compilation_database(tuple(context.s026_sources))
    context.files_database = context.run_root_path / "s026-identities-project.sqlite"
    context.facts_database = context.run_root_path / "s026-identities-facts.sqlite"
    imported = run([str(context.facts_tool), "import", "-v", "0", "--conf",
                    str(context.files_database), "--compilation-database",
                    str(context.run_root_path), *map(str, context.s026_sources)])
    require(imported.returncode == 0, imported.stdout + imported.stderr)


@when("all S-026 duplicate symbols are matched")
def match_duplicates(context: FactsToolContext) -> None:
    result = run([str(context.facts_tool), "match", "-v", "0", "--conf",
                  str(context.files_database), "--facts", str(context.facts_database),
                  "--matcher", 'functionDecl(anyOf(hasName("shared"),hasName("duplicate"))).bind("symbol")',
                  *map(str, context.s026_sources)])
    require(result.returncode == 0, result.stdout + result.stderr)


@then("same-name USRs stay distinct and one USR keeps both files")
def identity_shapes(context: FactsToolContext) -> None:
    duplicates = find(context, "--name", "duplicate")["matches"]
    require(len(duplicates) == 2 and len({row["usr"] for row in duplicates}) == 2, str(duplicates))
    shared = find(context, "--name", "shared")["matches"]
    require(len(shared) == 2 and len({row["usr"] for row in shared}) == 1, str(shared))
    require(len({row["file_id"] for row in shared}) == 2, json.dumps(shared))
    exact = find(context, "--usr", shared[0]["usr"])["matches"]
    require(exact == shared, f"exact USR lookup changed candidates: {exact}")
    filtered = find(context, "--name", "shared", "--kind", str(shared[0]["kind"]))["matches"]
    require(filtered == shared, f"kind filter changed candidates: {filtered}")


@given("the S-026 project is reset to schema version zero")
def version_zero(context: FactsToolContext) -> None:
    with sqlite3.connect(context.files_database) as connection:
        connection.executescript("DROP TABLE matched_symbol_index;"
                                 "UPDATE project_registry SET schema_version=0;"
                                 "PRAGMA user_version=37;")


@when("import reopens the S-026 version-zero project")
def reopen_version_zero(context: FactsToolContext) -> None:
    context.s026_migration = run([str(context.facts_tool), "import", "-v", "0", "--conf",
                                  str(context.files_database), "--facts", str(context.facts_database),
                                  "--compilation-database",
                                  str(context.run_root_path), str(context.targeted_match_source)])


@then("the project migration preserves facts user version and creates no candidates")
def migrated_version_zero(context: FactsToolContext) -> None:
    require(context.s026_migration.returncode == 0, context.s026_migration.stdout + context.s026_migration.stderr)
    with sqlite3.connect(context.files_database) as connection:
        require(connection.execute("SELECT schema_version FROM project_registry").fetchone() == (1,), "migration version")
        require(connection.execute("PRAGMA user_version").fetchone() == (37,), "facts user_version changed")
        require(connection.execute("SELECT count(*) FROM matched_symbol_index").fetchone() == (0,), "migration backfilled")


@when("an S-026 newer project schema is queried")
def query_newer(context: FactsToolContext) -> None:
    with sqlite3.connect(context.files_database) as connection:
        connection.execute("UPDATE project_registry SET schema_version=2")
    context.s026_newer_before = context.files_database.read_bytes()
    context.s026_newer = run([str(context.facts_tool), "symbol", "find", "-v", "0", "--conf",
                              str(context.files_database), "--name", "anything"])


@then("the unsupported project schema is reported without database writes")
def newer_rejected(context: FactsToolContext) -> None:
    output = context.s026_newer.stdout + context.s026_newer.stderr
    require(context.s026_newer.returncode == 1 and "unsupported-project-schema: 2" in output, output)
    require(context.files_database.read_bytes() == context.s026_newer_before, "newer schema was written")
