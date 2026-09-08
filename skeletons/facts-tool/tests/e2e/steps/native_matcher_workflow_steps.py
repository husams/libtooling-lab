from __future__ import annotations

import json
import shutil
import subprocess
import sqlite3

from pytest_bdd import given, then, when

from support import callgraph_run as cg
from support.database import require
from support.scenario import FactsToolContext


def run(command: list[str], cwd=None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, capture_output=True,
                          text=True, check=False)


def evidence_counts(path) -> tuple[int, int, int]:
    with sqlite3.connect(path) as db:
        counts = []
        for table in ("expression_occurrence", "source_region", "symbol"):
            try:
                counts.append(db.execute(
                    f"SELECT COUNT(*) FROM {table}").fetchone()[0])
            except sqlite3.OperationalError as error:
                if "no such table" not in str(error):
                    raise
                counts.append(0)
    return tuple(counts)


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
    context.expression_source = context.run_root_path / "expression_evidence.cpp"
    shutil.copy2(context.fixture_root / "expression_evidence.cpp",
                 context.expression_source)
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
    require({row[0] for row in rows} >= {
        "read", "write", "read_write", "escape", "unknown"}, str(rows))
    require(all(len(row[1]) == 64 and row[2] == "current" for row in rows), str(rows))


@when("the expression source changes and matching runs again")
def changed_expression_match(context: FactsToolContext) -> None:
    with context.expression_source.open("a", encoding="utf-8") as source:
        source.write("\n// source revision for evidence freshness\n")
    completed = run([
        str(context.facts_tool), "match", "-v", "0", "--conf",
        str(context.files_database), "--facts", str(context.facts_database),
        "--matcher", 'memberExpr(hasDeclaration(fieldDecl())).bind("expression")',
        str(context.expression_source),
    ])
    require(completed.returncode == 0, completed.stdout + completed.stderr)


@then("both source fingerprints remain queryable")
def both_expression_fingerprints(context: FactsToolContext) -> None:
    with sqlite3.connect(context.facts_database) as db:
        count = db.execute(
            "SELECT COUNT(DISTINCT source_sha256) FROM expression_occurrence"
        ).fetchone()[0]
    require(count >= 2, f"expected two expression source revisions, got {count}")


@when("a symbol matcher captures source regions")
def source_region_match(context: FactsToolContext) -> None:
    completed = run([
        str(context.facts_tool), "match", "-v", "0", "--capture-source",
        "--conf", str(context.files_database), "--facts",
        str(context.facts_database), "--matcher",
        'namedDecl(isExpansionInMainFile(), anyOf('
        'functionDecl(isDefinition(), unless(isImplicit())), '
        'cxxRecordDecl(isDefinition()))).bind("symbol")',
        str(context.expression_source),
    ])
    require(completed.returncode == 0, completed.stdout + completed.stderr)


@then("the source region rows retain definition ranges and freshness")
def source_region_rows(context: FactsToolContext) -> None:
    with sqlite3.connect(context.facts_database) as db:
        rows = db.execute(
            "SELECT sr.symbol_kind,s.qualified_name,sr.offset,sr.size,"
            "sr.source_sha256,sr.freshness FROM source_region sr JOIN symbol s "
            "ON s.id=sr.symbol_id WHERE sr.freshness='current'").fetchall()
    require(rows, "no source region rows")
    require({row[0] for row in rows} >= {"record", "method", "function"},
            str(rows))
    require(sum(row[1] == "expression_evidence::Record::overload"
                for row in rows) >= 2, str(rows))
    require(any(row[1] == "expression_evidence::Record::outOfLine"
                for row in rows), str(rows))
    require(any("::Nested" in row[1] for row in rows), str(rows))
    require(any(row[1] == "expression_evidence::Box" for row in rows), str(rows))
    require(any(row[1] == "expression_evidence::Derived" for row in rows), str(rows))
    require(all(row[2] >= 0 and row[3] > 0 and len(row[4]) == 64 and
                row[5] == "current" for row in rows), str(rows))


@when("a symbol matcher captures unsupported source regions")
def unsupported_source_region_match(context: FactsToolContext) -> None:
    completed = run([
        str(context.facts_tool), "match", "-v", "0", "--capture-source",
        "--conf", str(context.files_database), "--facts",
        str(context.facts_database), "--matcher",
        'namedDecl(isExpansionInMainFile(), anyOf(functionDecl(), '
        'cxxRecordDecl())).bind("symbol")',
        str(context.expression_source),
    ])
    require(completed.returncode == 0, completed.stdout + completed.stderr)


@then("unavailable source region reasons remain explicit")
def unavailable_source_region_rows(context: FactsToolContext) -> None:
    with sqlite3.connect(context.facts_database) as db:
        rows = db.execute(
            "SELECT freshness,unavailable_reason FROM source_region "
            "WHERE freshness='unavailable'").fetchall()
    require(rows, "no unavailable source region rows")
    reasons = {row[1] for row in rows}
    require(any("declaration-only" in reason for reason in reasons), str(rows))
    require(any("implicit" in reason for reason in reasons), str(rows))
    require(any("macro" in reason for reason in reasons), str(rows))


@given("a two-translation-unit expression evidence fixture")
def two_tu_expression_fixture(context: FactsToolContext) -> None:
    context.prepare()
    first = context.run_root_path / "expression_evidence.cpp"
    second = context.run_root_path / "expression_evidence_two.cpp"
    shutil.copy2(context.fixture_root / "expression_evidence.cpp", first)
    shutil.copy2(context.fixture_root / "expression_evidence_two.cpp", second)
    context.expression_sources = (first, second)
    context._write_compilation_database(context.expression_sources)
    context.files_database = context.run_root_path / "expression-project.sqlite"
    context.facts_database = context.run_root_path / "expression-facts.sqlite"
    imported = run([
        str(context.facts_tool), "import", "-v", "0", "--conf",
        str(context.files_database), "--compilation-database",
        str(context.run_root_path), *(str(source) for source in context.expression_sources),
    ])
    require(imported.returncode == 0, imported.stdout + imported.stderr)


@when("an expression matcher captures both translation units")
def two_tu_expression_match(context: FactsToolContext) -> None:
    completed = run([
        str(context.facts_tool), "match", "-v", "0", "--conf",
        str(context.files_database), "--facts", str(context.facts_database),
        "--matcher", 'memberExpr(hasDeclaration(fieldDecl())).bind("expression")',
        *(str(source) for source in context.expression_sources),
    ])
    require(completed.returncode == 0, completed.stdout + completed.stderr)


@then("each translation unit retains its own source fingerprint")
def two_tu_fingerprints(context: FactsToolContext) -> None:
    with sqlite3.connect(context.facts_database) as db:
        rows = db.execute(
            "SELECT COUNT(DISTINCT file_id), COUNT(DISTINCT source_sha256) "
            "FROM expression_occurrence").fetchone()
    require(rows[0] >= 2 and rows[1] >= 2, str(rows))


@when("a project pair omits one evidence translation unit")
def mismatched_expression_pair(context: FactsToolContext) -> None:
    alternate_conf = context.run_root_path / "alternate-project.sqlite"
    alternate_root = context.run_root_path / "alternate-project"
    alternate_root.mkdir()
    alternate_source = alternate_root / context.expression_sources[0].name
    shutil.copy2(context.expression_sources[0], alternate_source)
    original_run_root = context.run_root
    context.run_root = alternate_root
    context._write_compilation_database((alternate_source,))
    context.run_root = original_run_root
    imported = run([
        str(context.facts_tool), "import", "-v", "0", "--conf",
        str(alternate_conf), "--compilation-database",
        str(alternate_root), str(alternate_source),
    ])
    require(imported.returncode == 0, imported.stdout + imported.stderr)
    completed = run([
        str(context.facts_tool), "match", "-v", "0", "--conf",
        str(alternate_conf), "--facts", str(context.facts_database),
        "--matcher", 'memberExpr(hasDeclaration(fieldDecl())).bind("expression")',
        str(alternate_source),
    ])
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr


@then("pair validation rejects the evidence store")
def mismatched_expression_pair_rejected(context: FactsToolContext) -> None:
    require(context.last_returncode != 0, context.last_output)
    require("pair" in context.last_output.lower() or
            "provenance" in context.last_output.lower() or
            "incompatible-symbol-universe" in context.last_output.lower(),
            context.last_output)


@given("two unavailable expression fixtures with equal source offsets")
def unavailable_expression_fixtures(context: FactsToolContext) -> None:
    context.prepare()
    first = context.run_root_path / "expression_unavailable_one.cpp"
    second = context.run_root_path / "expression_unavailable_two.cpp"
    shutil.copy2(context.fixture_root / "expression_unavailable_one.cpp", first)
    shutil.copy2(context.fixture_root / "expression_unavailable_two.cpp", second)
    context.expression_sources = (first, second)
    context._write_compilation_database(context.expression_sources)
    context.files_database = context.run_root_path / "unavailable-project.sqlite"
    context.facts_database = context.run_root_path / "unavailable-facts.sqlite"
    imported = run([
        str(context.facts_tool), "import", "-v", "0", "--conf",
        str(context.files_database), "--compilation-database",
        str(context.run_root_path), *(str(source) for source in context.expression_sources),
    ])
    require(imported.returncode == 0, imported.stdout + imported.stderr)


@when("an expression matcher captures unavailable field evidence")
def unavailable_expression_match(context: FactsToolContext) -> None:
    completed = run([
        str(context.facts_tool), "match", "-v", "0", "--conf",
        str(context.files_database), "--facts", str(context.facts_database),
        "--matcher", 'memberExpr(hasDeclaration(fieldDecl())).bind("expression")',
        *(str(source) for source in context.expression_sources),
    ])
    require(completed.returncode == 0, completed.stdout + completed.stderr)


@then("unavailable occurrences remain distinct with explicit reasons")
def unavailable_expression_rows(context: FactsToolContext) -> None:
    with sqlite3.connect(context.facts_database) as db:
        rows = db.execute(
            "SELECT identity,freshness,unavailable_reason FROM "
            "expression_occurrence").fetchall()
    require(len(rows) >= 2 and len({row[0] for row in rows}) == len(rows), str(rows))
    require(all(row[1] == "unavailable" and row[2] for row in rows), str(rows))


@when("the expression facts store becomes read-only for a second match")
def readonly_expression_match(context: FactsToolContext) -> None:
    with sqlite3.connect(context.facts_database) as db:
        before = db.execute(
            "SELECT COUNT(*) FROM expression_occurrence").fetchone()[0]
    context.facts_database.chmod(0o444)
    try:
        completed = run([
            str(context.facts_tool), "match", "-v", "0", "--conf",
            str(context.files_database), "--facts", str(context.facts_database),
            "--matcher", 'memberExpr(hasDeclaration(fieldDecl())).bind("expression")',
            str(context.expression_source),
        ])
    finally:
        context.facts_database.chmod(0o644)
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr
    context.expression_before_failure = before


@when("a valid then invalid expression matcher runs")
def valid_then_invalid_expression_match(context: FactsToolContext) -> None:
    invalid = context.run_root_path / "expression_match_failure.cpp"
    shutil.copy2(context.fixture_root / "expression_match_failure.cpp", invalid)
    context._write_compilation_database((context.expression_source, invalid))
    imported = run([
        str(context.facts_tool), "import", "-v", "0", "--conf",
        str(context.files_database), "--compilation-database",
        str(context.run_root_path), str(invalid),
    ])
    require(imported.returncode == 0, imported.stdout + imported.stderr)
    context.expression_counts_before = evidence_counts(context.facts_database)
    completed = run([
        str(context.facts_tool), "match", "-v", "0", "--conf",
        str(context.files_database), "--facts", str(context.facts_database),
        "--matcher", 'memberExpr(hasDeclaration(fieldDecl())).bind("expression")',
        str(context.expression_source), str(invalid),
    ])
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr


@then("the failed expression match leaves every facts table unchanged")
def failed_expression_match_unchanged(context: FactsToolContext) -> None:
    require(context.last_returncode != 0, context.last_output)
    after = evidence_counts(context.facts_database)
    require(after == context.expression_counts_before,
            f"before={context.expression_counts_before} after={after}")


@when("ordinary extraction runs for the expression fixture")
def ordinary_expression_extract(context: FactsToolContext) -> None:
    completed = run([
        str(context.facts_tool), "extract", "-v", "0", "--conf",
        str(context.files_database), "--output", str(context.facts_database),
        str(context.expression_source),
    ])
    require(completed.returncode == 0, completed.stdout + completed.stderr)


@then("ordinary extraction has no expression or source evidence rows")
def ordinary_expression_rows_absent(context: FactsToolContext) -> None:
    with sqlite3.connect(context.facts_database) as db:
        rows = db.execute(
            "SELECT (SELECT COUNT(*) FROM expression_occurrence), "
            "(SELECT COUNT(*) FROM source_region)").fetchone()
    require(rows == (0, 0), str(rows))


@then("the failed expression match leaves the prior evidence unchanged")
def readonly_expression_result(context: FactsToolContext) -> None:
    require(context.last_returncode != 0, context.last_output)
    with sqlite3.connect(context.facts_database) as db:
        after = db.execute(
            "SELECT COUNT(*) FROM expression_occurrence").fetchone()[0]
    require(after == context.expression_before_failure,
            str((after, context.expression_before_failure)))


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
