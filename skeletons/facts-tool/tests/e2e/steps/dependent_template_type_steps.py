from __future__ import annotations

import json
import subprocess
from pathlib import Path

from pytest_bdd import given, then, when
from support.database import query, require
from support.scenario import FactsToolContext


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True, check=False)


@given(
    "a reproducing compile database for dependent template parameter types",
    target_fixture="dependent_template_source",
)
def given_dependent_template_compile_database(context: FactsToolContext) -> Path:
    context.prepare()
    source = (context.fixture_root / "dependent_template_types.cpp").resolve(
        strict=True
    )
    context.facts_database = context.run_root_path / "dependent-template-facts.sqlite"
    context.files_database = context.run_root_path / "dependent-template-project.sqlite"
    compilation_database = [
        {
            "directory": str(context.fixture_root),
            "file": str(source),
            "arguments": [str(context.compiler), "-std=c++23", "-c", str(source)],
        }
    ]
    (context.run_root_path / "compile_commands.json").write_text(
        json.dumps(compilation_database, indent=2) + "\n", encoding="utf-8"
    )
    return source


def run_subcommand_extraction(
    context: FactsToolContext, source: Path
) -> subprocess.CompletedProcess[str]:
    imported = run(
        [
            str(context.facts_tool),
            "import",
            "--conf",
            str(context.files_database_path),
            "--compilation-database",
            str(context.run_root_path),
            str(source),
        ]
    )
    require(
        imported.returncode == 0,
        f"expected import exit code 0, got {imported.returncode}:\n"
        + imported.stdout
        + imported.stderr,
    )
    return run(
        [
            str(context.facts_tool),
            "extract",
            "--output",
            str(context.facts_database_path),
            "--conf",
            str(context.files_database_path),
            str(source),
        ]
    )


@when("the real extraction command indexes the dependent-template fixture")
def when_extract_indexes_dependent_templates(
    context: FactsToolContext, dependent_template_source: Path
) -> None:
    completed = run_subcommand_extraction(context, dependent_template_source)
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr


@then(
    "the dependent-template extraction exits successfully without incomplete diagnostics"
)
def then_dependent_template_extraction_succeeds(context: FactsToolContext) -> None:
    require(
        context.last_returncode == 0,
        f"expected extract exit code 0, got {context.last_returncode}:\n"
        + context.last_output,
    )
    require(
        "indexing incomplete" not in context.last_output,
        f"unexpected incomplete diagnostic:\n{context.last_output}",
    )


@then("dependent template parameters retain resolvable types and modifiers")
def then_dependent_template_parameters_are_resolvable(
    context: FactsToolContext,
) -> None:
    parameters = query(
        context.facts_database_path,
        "SELECT source.qualified_name,target.qualified_name,p.is_pointer,"
        "p.is_lvalue_reference,p.is_rvalue_reference,p.is_const "
        "FROM parameter p "
        "JOIN symbol source ON source.id=p.symbol_id "
        "JOIN symbol target ON target.id=p.type "
        "WHERE source.qualified_name LIKE 'b004::%' "
        "AND EXISTS (SELECT 1 FROM template_argument declared "
        "            WHERE declared.symbol_id=source.id)",
    )
    expected = {
        ("b004::identity", "T", 0, 0, 0, 0),
        ("b004::pointer", "T", 1, 0, 0, 0),
        ("b004::lvalue", "T", 0, 1, 0, 1),
        ("b004::rvalue", "T", 0, 0, 1, 0),
        ("b004::Nested::relay", "U", 0, 0, 0, 0),
        ("b004::Nested<b004::Widget>::relay", "U", 0, 0, 0, 0),
    }
    require(
        expected <= set(parameters),
        f"missing dependent template parameter facts: {expected - set(parameters)}",
    )


@then("dependent function-template instances point to their primary templates")
def then_dependent_instances_point_to_primary(context: FactsToolContext) -> None:
    relations = set(
        query(
            context.facts_database_path,
            "SELECT DISTINCT source.qualified_name,relation.kind "
            "FROM relation "
            "JOIN symbol source ON source.id=relation.source_id "
            "JOIN symbol destination ON destination.id=relation.destination_id "
            "WHERE relation.kind IN (4,5) "
            "AND source.qualified_name=destination.qualified_name "
            "AND source.qualified_name LIKE 'b004::%'",
        )
    )
    expected = {
        (name, 5)
        for name in (
            "b004::identity",
            "b004::pointer",
            "b004::lvalue",
            "b004::rvalue",
        )
    }
    expected.add(("b004::identity", 4))
    require(
        expected <= relations,
        f"missing dependent template relations: {expected - relations}",
    )
