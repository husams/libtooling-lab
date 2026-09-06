from __future__ import annotations

import sqlite3

from pytest_bdd import given, then, when

from steps.matched_symbol_index_steps import find
from steps.native_matcher_workflow_steps import run
from support.database import require
from support.scenario import FactsToolContext


@given("an S-026 invalid-USR matcher project")
def invalid_usr_project(context: FactsToolContext) -> None:
    context.prepare()
    context.s026_invalid_source = context.fixture_root / "invalid_usr_declarations.cpp"
    context._write_compilation_database((context.s026_invalid_source,))
    context.files_database = context.run_root_path / "s026-invalid-project.sqlite"
    context.facts_database = context.run_root_path / "s026-invalid-facts.sqlite"
    result = run([str(context.facts_tool), "import", "-v", "0", "--conf",
                  str(context.files_database), "--compilation-database",
                  str(context.run_root_path), str(context.s026_invalid_source)])
    require(result.returncode == 0, result.stdout + result.stderr)


@when("the S-026 invalid identity is matched")
def match_invalid_identity(context: FactsToolContext) -> None:
    context.s026_invalid = run([
        str(context.facts_tool), "match", "-v", "0", "--conf",
        str(context.files_database), "--facts", str(context.facts_database),
        "--matcher", 'fieldDecl(unless(hasName("named"))).bind("symbol")',
        str(context.s026_invalid_source),
    ])


@then("matching reports invalid-usr and publishes no candidate")
def invalid_identity_not_indexed(context: FactsToolContext) -> None:
    output = context.s026_invalid.stdout + context.s026_invalid.stderr
    require(context.s026_invalid.returncode == 1 and "invalid-usr" in output, output)
    require(find(context, "--name", "probe::Bits")["matches"] == [], "invalid row published")


@when("an S-026 unregistered header declaration is matched")
def match_unregistered_header(context: FactsToolContext) -> None:
    with sqlite3.connect(context.files_database) as connection:
        connection.execute("DELETE FROM file WHERE name <> 'targeted_match.cpp'")
    context.s026_unregistered = run([
        str(context.facts_tool), "match", "-v", "0", "--conf",
        str(context.files_database), "--facts", str(context.facts_database),
        "--matcher",
        'cxxRecordDecl(hasName("std::basic_string"),isDefinition()).bind("symbol")',
        str(context.targeted_match_source),
    ])


@then("matching reports the incomplete project and publishes no candidate")
def unregistered_header_not_indexed(context: FactsToolContext) -> None:
    output = context.s026_unregistered.stdout + context.s026_unregistered.stderr
    require(context.s026_unregistered.returncode == 1 and
            "project configuration is incomplete" in output, output)
    require(find(context, "--name", "std::basic_string")["matches"] == [],
            "unregistered row published")


@when("the matched index is cleared with file ID zero")
def clear_invalid_file(context: FactsToolContext) -> None:
    context.s026_before_invalid_clear = find(context, "--name", "targeted_match")["matches"]
    context.s026_invalid_clear = run([
        str(context.facts_tool), "symbol", "index", "clear", "-v", "0",
        "--conf", str(context.files_database), "--file-id", "0",
    ])


@then("the invalid file ID is rejected without changing candidates")
def invalid_file_rejected(context: FactsToolContext) -> None:
    require(context.s026_invalid_clear.returncode == 2,
            context.s026_invalid_clear.stdout + context.s026_invalid_clear.stderr)
    require(find(context, "--name", "targeted_match")["matches"] ==
            context.s026_before_invalid_clear, "invalid clear changed candidates")
