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
    "a reproducing compile database for filtered external targets",
    target_fixture="external_target_source",
)
def given_external_target_compile_database(context: FactsToolContext) -> Path:
    context.prepare()
    source = (context.fixture_root / "external_targets.cpp").resolve(strict=True)
    context.facts_database = context.run_root_path / "external-targets.sqlite"
    context.files_database = context.run_root_path / "external-targets-project.sqlite"
    compilation_database = [
        {
            "directory": str(context.fixture_root),
            "file": str(source),
            "arguments": [
                str(context.compiler),
                "-std=c++23",
                "-c",
                str(source),
            ],
        }
    ]
    (context.run_root_path / "compile_commands.json").write_text(
        json.dumps(compilation_database, indent=2) + "\n", encoding="utf-8"
    )
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
        "cannot import reproducing compile database:\n"
        + imported.stdout
        + imported.stderr,
    )
    return source


@when("the real extract subcommand indexes the external-target fixture")
def when_extract_indexes_external_targets(
    context: FactsToolContext, external_target_source: Path
) -> None:
    completed = run(
        [
            str(context.facts_tool),
            "extract",
            "-o",
            str(context.facts_database_path),
            "-c",
            str(context.files_database_path),
            str(external_target_source),
        ]
    )
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr


@then(
    "the external-target extraction exits successfully without incomplete diagnostics"
)
def then_external_target_extraction_succeeds(context: FactsToolContext) -> None:
    require(
        context.last_returncode == 0,
        f"expected extract exit code 0, got {context.last_returncode}:\n"
        + context.last_output,
    )
    require(
        "indexing incomplete" not in context.last_output,
        f"unexpected incomplete diagnostic:\n{context.last_output}",
    )


@then("RelationResult aliases a lightweight external std::expected symbol")
def then_alias_target_is_lightweight_external(context: FactsToolContext) -> None:
    relation = query(
        context.facts_database_path,
        "SELECT destination.qualified_name,destination.is_external,"
        "destination.is_definition FROM relation r "
        "JOIN symbol source ON source.id=r.source_id "
        "JOIN symbol destination ON destination.id=r.destination_id "
        "WHERE r.kind=19 "
        "AND source.qualified_name='regression::RelationResult'",
    )
    require(
        relation == [("std::expected", 1, 0)],
        f"unexpected external alias target: {relation}",
    )
    external_expected = query(
        context.facts_database_path,
        "SELECT qualified_name,is_definition FROM symbol "
        "WHERE qualified_name='std::expected' AND is_external=1",
    )
    require(
        external_expected == [("std::expected", 0)],
        f"std::expected header body was indexed: {external_expected}",
    )


@then("compound external field types resolve to lightweight symbols")
def then_compound_external_field_types_are_persisted(
    context: FactsToolContext,
) -> None:
    relations = query(
        context.facts_database_path,
        "SELECT source.qualified_name,destination.qualified_name,"
        "destination.is_external,destination.is_definition "
        "FROM relation r "
        "JOIN symbol source ON source.id=r.source_id "
        "JOIN symbol destination ON destination.id=r.destination_id "
        "WHERE r.kind=20 "
        "AND source.qualified_name LIKE 'regression::ExternalValues::%' "
        "ORDER BY source.qualified_name",
    )
    require(
        relations
        == [
            ("regression::ExternalValues::result", "std::expected", 1, 0),
            ("regression::ExternalValues::view", "std::string_view", 1, 0),
        ],
        f"unexpected compound external field relations: {relations}",
    )


@then("the compound external parameter types retain their modifiers")
def then_compound_parameter_types_are_persisted(
    context: FactsToolContext,
) -> None:
    parameters = query(
        context.facts_database_path,
        "SELECT s.qualified_name,t.qualified_name,t.is_external,"
        "t.is_definition,p.is_pointer,p.is_lvalue_reference,"
        "p.is_rvalue_reference,p.is_const "
        "FROM parameter p "
        "JOIN symbol s ON s.id=p.symbol_id "
        "JOIN symbol t ON t.id=p.type "
        "WHERE s.qualified_name LIKE 'regression::%' "
        "AND t.qualified_name IN ('std::expected','std::string_view','uint16_t') "
        "ORDER BY s.qualified_name,p.position",
    )
    require(
        parameters
        == [
            (
                "regression::externalSymbol",
                "std::expected",
                1,
                0,
                1,
                0,
                0,
                1,
            ),
            (
                "regression::extractInheritanceRelation",
                "std::expected",
                1,
                0,
                0,
                0,
                1,
                0,
            ),
            (
                "regression::extractInheritanceRelation",
                "uint16_t",
                1,
                0,
                0,
                0,
                0,
                0,
            ),
            (
                "regression::findOrStoreInheritanceTarget",
                "std::expected",
                1,
                0,
                0,
                1,
                0,
                1,
            ),
            (
                "regression::inheritanceFailure",
                "std::string_view",
                1,
                0,
                0,
                0,
                0,
                0,
            ),
        ],
        f"unexpected compound external parameters: {parameters}",
    )
