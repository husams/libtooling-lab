from __future__ import annotations

import subprocess

from pytest_bdd import given, then, when

from support.database import require
from support.scenario import FactsToolContext


def run(command: list[str], cwd=None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, capture_output=True,
                          text=True, check=False)


def prepare_pair(context: FactsToolContext) -> None:
    context.prepare()
    context.targeted_match_source = (
        context.fixture_root / "targeted_match.cpp").resolve(strict=True)
    context._write_compilation_database((context.targeted_match_source,))
    context.files_database = context.run_root_path / "matcher-project.sqlite"
    context.facts_database = context.run_root_path / "matcher-facts.sqlite"
    imported = run([
        str(context.facts_tool), "import", "-v", "0", "--conf",
        str(context.files_database), "--compilation-database",
        str(context.run_root_path), str(context.targeted_match_source),
    ])
    require(imported.returncode == 0, imported.stdout + imported.stderr)
    extracted = run([
        str(context.facts_tool), "extract", "-v", "0", "--conf",
        str(context.files_database), "--output",
        str(context.facts_database), str(context.targeted_match_source),
    ])
    require(extracted.returncode == 0, extracted.stdout + extracted.stderr)


@given("a separately stored native matcher fixture")
def separate_fixture(context: FactsToolContext) -> None:
    prepare_pair(context)


@when("a symbol matcher runs with the explicit database pair")
def pair_symbol_match(context: FactsToolContext) -> None:
    completed = run([
        str(context.facts_tool), "match", "-v", "0", "--conf",
        str(context.files_database), "--facts", str(context.facts_database),
        "--matcher",
        'functionDecl(hasName("targeted_match::caller")).bind("symbol")',
        str(context.targeted_match_source),
    ])
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr


@then("the paired native match succeeds")
def paired_match_succeeds(context: FactsToolContext) -> None:
    require(context.last_returncode == 0, context.last_output)
    require("symbol kind=function" in context.last_output, context.last_output)


@then("the native call graph can traverse the matched facts")
def paired_call_graph(context: FactsToolContext) -> None:
    completed = run([
        str(context.facts_tool), "analyse", "call-graph", "-v", "0",
        "--facts", str(context.facts_database), "--function",
        "targeted_match::caller",
    ])
    output = completed.stdout + completed.stderr
    require(completed.returncode == 0, output)
    require("targeted_match::print" in output, output)


@when("an invalid symbol binding runs with the explicit database pair")
def invalid_pair_match(context: FactsToolContext) -> None:
    completed = run([
        str(context.facts_tool), "match", "-v", "0", "--conf",
        str(context.files_database), "--facts", str(context.facts_database),
        "--matcher",
        'functionDecl(hasName("targeted_match::caller")).bind("source")',
        str(context.targeted_match_source),
    ])
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr


@then("the paired native match fails with an actionable binding contract")
def invalid_binding_message(context: FactsToolContext) -> None:
    require(context.last_returncode != 0, context.last_output)
    require("supported contract" in context.last_output, context.last_output)


@given("a separate-build native matcher fixture")
def separate_build_fixture(context: FactsToolContext) -> None:
    context.prepare()
    context.targeted_match_source = (
        context.fixture_root / "targeted_match.cpp").resolve(strict=True)
    context._write_compilation_database((context.targeted_match_source,))
    context.files_database = context.run_root_path / "relative-project.sqlite"


@when("import runs with a relative source selector")
def relative_import(context: FactsToolContext) -> None:
    completed = run([
        str(context.facts_tool), "import", "-v", "0", "--conf",
        str(context.files_database), "--compilation-database",
        str(context.run_root_path), "targeted_match.cpp",
    ], cwd=context.fixture_root)
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr


@then("the relative native import succeeds")
def relative_import_succeeds(context: FactsToolContext) -> None:
    require(context.last_returncode == 0, context.last_output)
    require("Imported 1 compile command(s)" in context.last_output,
            context.last_output)
