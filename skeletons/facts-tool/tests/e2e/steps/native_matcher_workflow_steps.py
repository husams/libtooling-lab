from __future__ import annotations

import json
import subprocess
import sqlite3

from pytest_bdd import given, then, when

from support import callgraph_run as cg
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
    context.facts_database = context.run_root_path / "nested" / "matcher-facts.sqlite"
    imported = run([
        str(context.facts_tool), "import", "-v", "0", "--conf",
        str(context.files_database), "--compilation-database",
        str(context.run_root_path), str(context.targeted_match_source),
    ])
    require(imported.returncode == 0, imported.stdout + imported.stderr)


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


@when("a direct-call matcher runs twice with the explicit database pair")
def paired_direct_call(context: FactsToolContext) -> None:
    command = [
        str(context.facts_tool), "match", "-v", "0", "--conf",
        str(context.files_database), "--facts", str(context.facts_database),
        "--matcher",
        'callExpr(callee(functionDecl(hasName("targeted_match::print")).bind("callee"))).bind("call")',
        str(context.targeted_match_source),
    ]
    first = run(command)
    require(first.returncode == 0, first.stdout + first.stderr)
    second = run(command)
    require(second.returncode == 0, second.stdout + second.stderr)


@given("a separately stored expression evidence fixture")
def expression_fixture(context: FactsToolContext) -> None:
    context.prepare()
    context.expression_source = (
        context.fixture_root / "expression_evidence.cpp").resolve(strict=True)
    context._write_compilation_database((context.expression_source,))
    context.files_database = context.run_root_path / "expression-project.sqlite"
    context.facts_database = context.run_root_path / "expression-facts.sqlite"
    imported = run([
        str(context.facts_tool), "import", "-v", "0", "--conf",
        str(context.files_database), "--compilation-database",
        str(context.run_root_path), str(context.expression_source),
    ])
    require(imported.returncode == 0, imported.stdout + imported.stderr)


@when("an expression matcher captures field evidence")
def expression_match(context: FactsToolContext) -> None:
    completed = run([
        str(context.facts_tool), "match", "-v", "0", "--conf",
        str(context.files_database), "--facts", str(context.facts_database),
        "--matcher",
        'memberExpr(hasDeclaration(fieldDecl())).bind("expression")',
        str(context.expression_source),
    ])
    require(completed.returncode == 0, completed.stdout + completed.stderr)


@then("the expression evidence rows record direct field effects and a source fingerprint")
def expression_rows(context: FactsToolContext) -> None:
    with sqlite3.connect(context.facts_database) as db:
        rows = db.execute(
            "SELECT access,source_sha256,freshness FROM expression_occurrence "
            "WHERE target_id IS NOT NULL").fetchall()
    require(rows, "no expression evidence rows")
    require({row[0] for row in rows} >= {"read", "write", "read_write"}, str(rows))
    require(all(len(row[1]) == 64 and row[2] == "current" for row in rows), str(rows))


@when("a symbol matcher captures source regions")
def source_region_match(context: FactsToolContext) -> None:
    completed = run([
        str(context.facts_tool), "match", "-v", "0", "--capture-source",
        "--conf", str(context.files_database), "--facts",
        str(context.facts_database), "--matcher",
        'cxxRecordDecl(isDefinition()).bind("symbol")',
        str(context.expression_source),
    ])
    require(completed.returncode == 0, completed.stdout + completed.stderr)


@then("the source region rows retain definition ranges and freshness")
def source_region_rows(context: FactsToolContext) -> None:
    with sqlite3.connect(context.facts_database) as db:
        rows = db.execute(
            "SELECT offset,size,source_sha256,freshness FROM source_region "
            "WHERE symbol_kind='record'").fetchall()
    require(rows, "no source region rows")
    require(all(row[0] >= 0 and row[1] > 0 and len(row[2]) == 64 and
                row[3] == "current" for row in rows), str(rows))


@then("the native call graph can traverse the matched facts twice")
def paired_call_graph(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    first = cg.run_graph(context, "--function", "targeted_match::caller", conf=False)
    second = cg.run_graph(context, "--function", "targeted_match::caller", conf=False)
    first_id, _ = cg.completion(first)
    second_id, _ = cg.completion(second)
    require(first.returncode == 0, first.stdout + first.stderr)
    require(second.returncode == 0, second.stdout + second.stderr)
    first_edges = cg.edge_names(facts, first_id)
    second_edges = cg.edge_names(facts, second_id)
    require(first_edges == second_edges, str((first_edges, second_edges)))
    require(("targeted_match::caller", "targeted_match::print") in first_edges,
            str(first_edges))


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
    require(
        'bind("symbol")' in context.last_output and
        'bind("call")+bind("callee")' in context.last_output and
        'bind("source")+bind("target")' in context.last_output,
        context.last_output,
    )


@then("the matcher help lists the supported binding contracts")
def matcher_help_contracts(context: FactsToolContext) -> None:
    completed = run([str(context.facts_tool), "match", "--help"])
    output = completed.stdout + completed.stderr
    require(completed.returncode == 0, output)
    require("bind symbol" in output, output)
    require("call+callee" in output, output)
    require("source+target[+site]" in output, output)


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
        str(context.run_root_path), "--component", "app=.",
        "targeted_match.cpp",
    ], cwd=context.fixture_root)
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr
    require(completed.returncode == 0, context.last_output)
    context.relative_commands = export_commands(context, context.files_database)


def export_commands(context: FactsToolContext, database) -> list[dict]:
    completed = run([
        str(context.facts_tool), "component", "compile-commands", "-v", "0",
        "--conf", str(database), "app",
    ])
    output = completed.stdout + completed.stderr
    require(completed.returncode == 0, output)
    return json.loads(completed.stdout)


@when("import runs with an absolute source selector")
def absolute_import(context: FactsToolContext) -> None:
    context.absolute_files_database = context.run_root_path / "absolute-project.sqlite"
    completed = run([
        str(context.facts_tool), "import", "-v", "0", "--conf",
        str(context.absolute_files_database), "--compilation-database",
        str(context.run_root_path), "--component", "app=.",
        str(context.targeted_match_source),
    ])
    context.absolute_import_output = completed.stdout + completed.stderr
    require(completed.returncode == 0, context.absolute_import_output)
    context.absolute_commands = export_commands(
        context, context.absolute_files_database)


@then("relative and absolute native imports select the same command")
def equivalent_imports(context: FactsToolContext) -> None:
    require("Imported 1 compile command(s)" in context.last_output,
            context.last_output)
    require("Imported 1 compile command(s)" in context.absolute_import_output,
            context.absolute_import_output)
    require(context.relative_commands == context.absolute_commands,
            f"relative={context.relative_commands} absolute={context.absolute_commands}")


@when("import runs with an invalid relative source selector")
def invalid_relative_import(context: FactsToolContext) -> None:
    completed = run([
        str(context.facts_tool), "import", "-v", "0", "--conf",
        str(context.run_root_path / "invalid-project.sqlite"),
        "--compilation-database", str(context.run_root_path), "missing.cpp",
    ], cwd=context.fixture_root)
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr


@then("the invalid native selector reports its invocation base")
def invalid_selector_reports_base(context: FactsToolContext) -> None:
    require(context.last_returncode != 0, context.last_output)
    require("relative selectors are resolved relative to the invocation directory"
            in context.last_output, context.last_output)
    require(str(context.fixture_root / "missing.cpp") in context.last_output,
            context.last_output)


@given("a default-configured native matcher fixture")
def default_fixture(defaults, context: FactsToolContext) -> None:
    source = defaults.cwd / "main.cpp"
    source.write_text("namespace defaults_match { void run() {} }\n",
                      encoding="utf-8")
    (defaults.cwd / "compile_commands.json").write_text(json.dumps([{
        "directory": str(defaults.cwd),
        "file": str(source),
        "arguments": [str(context.compiler), "-std=c++23", str(source)],
    }]))
    defaults.write(
        conf_root=str(defaults.root / "store"),
        conf_template="project.db",
        facts_template="facts/{project_name}.db",
    )
    imported = defaults.run("import", "--component", "app=.",
                            "-p", defaults.cwd)
    require(imported.returncode == 0, imported.stdout + imported.stderr)
    defaults.native_source = source


@when("a native matcher runs with configured defaults")
def default_match(defaults) -> None:
    completed = defaults.run(
        "match", "-v", "0", "--matcher",
        'functionDecl(hasName("defaults_match::run")).bind("symbol")',
        "main.cpp",
    )
    defaults.last = completed


@then("the default native match succeeds and materializes facts_template")
def default_match_succeeds(defaults) -> None:
    require(defaults.last.returncode == 0,
            defaults.last.stdout + defaults.last.stderr)
    require("symbol kind=function" in defaults.last.stdout,
            defaults.last.stdout + defaults.last.stderr)
    databases = list(defaults.root.rglob("*.db"))
    require(len(databases) >= 2, str(databases))
    require(any("facts" in path.parts for path in databases), str(databases))


@then("the relative native import succeeds")
def relative_import_succeeds(context: FactsToolContext) -> None:
    require(context.last_returncode == 0, context.last_output)
    require("Imported 1 compile command(s)" in context.last_output,
            context.last_output)
