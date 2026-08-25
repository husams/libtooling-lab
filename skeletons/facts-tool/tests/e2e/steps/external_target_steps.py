from __future__ import annotations

import json
import os
import shutil
import subprocess
from pathlib import Path

import pytest
from pytest_bdd import given, then, when
from support.database import query, require
from support.scenario import FactsToolContext


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True, check=False)


def prepare_compile_database(
    context: FactsToolContext,
    fixture_name: str,
    database_stem: str,
    driver: Path | None = None,
    expected_library: str | None = None,
) -> Path:
    context.prepare()
    source = (context.fixture_root / fixture_name).resolve(strict=True)
    context.facts_database = context.run_root_path / f"{database_stem}.sqlite"
    context.files_database = context.run_root_path / f"{database_stem}-project.sqlite"
    selected_driver = driver or context.compiler
    expected_option = {
        "libstdc++": "-DFACTS_EXPECT_LIBSTDCXX",
        "libc++": "-DFACTS_EXPECT_LIBCPP",
    }.get(expected_library)
    arguments = [str(selected_driver), "-std=c++23", "-c"]
    if expected_option:
        arguments.append(expected_option)
    arguments.append(str(source))
    compilation_database = [
        {
            "directory": str(context.fixture_root),
            "file": str(source),
            "arguments": arguments,
        }
    ]
    (context.run_root_path / "compile_commands.json").write_text(
        json.dumps(compilation_database, indent=2) + "\n", encoding="utf-8"
    )
    return source


def compiler_output(driver: Path) -> str:
    return run([str(driver), "--version"]).stdout.lower()


def selected_standard_library(driver: Path) -> str:
    completed = subprocess.run(
        [str(driver), "-dM", "-E", "-x", "c++", "-"],
        input="#include <string_view>\n",
        capture_output=True,
        text=True,
        check=False,
    )
    require(
        completed.returncode == 0,
        f"cannot probe target standard library for {driver}:\n{completed.stderr}",
    )
    if "__GLIBCXX__" in completed.stdout:
        return "libstdc++"
    if "_LIBCPP_VERSION" in completed.stdout:
        return "libc++"
    raise AssertionError(f"target driver selected no known C++ library: {driver}")


def target_driver(environment: str, fallback: str) -> Path | None:
    configured = os.environ.get(environment)
    found = configured or shutil.which(fallback)
    return Path(found).resolve() if found else None


@given(
    "a reproducing compile database for filtered external targets",
    target_fixture="external_target_source",
)
def given_external_target_compile_database(context: FactsToolContext) -> Path:
    return prepare_compile_database(context, "external_targets.cpp", "external-targets")


@given(
    "a reproducing clang++ compile database for filtered external targets",
    target_fixture="external_target_source",
)
def given_clang_external_target_compile_database(
    context: FactsToolContext,
) -> Path:
    configured = Path(context.compiler)
    driver = (
        configured.resolve()
        if "clang++" in configured.name
        else target_driver("FACTS_CLANGXX", "clang++")
    )
    if driver is None:
        pytest.skip("clang++ target driver is not installed")
    expected = selected_standard_library(driver)
    return prepare_compile_database(
        context, "toolchain_targets.cpp", "external-targets-clang", driver, expected
    )


@given(
    "a reproducing GNU g++ compile database for filtered external targets",
    target_fixture="external_target_source",
)
def given_gnu_external_target_compile_database(
    context: FactsToolContext,
) -> Path:
    driver = target_driver("FACTS_GXX", "g++")
    if driver is None or "clang" in compiler_output(driver):
        pytest.skip("a GNU g++ target driver is not installed")
    return prepare_compile_database(
        context, "toolchain_targets.cpp", "external-targets-gxx", driver, "libstdc++"
    )


@given(
    "a reproducing compile database for a filtered external template primary",
    target_fixture="external_template_specialization_source",
)
def given_external_template_specialization_compile_database(
    context: FactsToolContext,
) -> Path:
    return prepare_compile_database(
        context,
        "external_template_specialization.cpp",
        "external-template-specialization",
    )


@when("the real extraction command indexes the external-target fixture")
def when_extract_indexes_external_targets(
    context: FactsToolContext, external_target_source: Path
) -> None:
    extract_fixture(context, external_target_source)


def extract_fixture(context: FactsToolContext, source: Path) -> None:
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
    completed = run(
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
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr


@then("the stored external-target driver is preserved")
def then_stored_external_target_driver_is_preserved(
    context: FactsToolContext,
) -> None:
    rows = query(
        context.files_database_path,
        "SELECT driver FROM file WHERE driver IS NOT NULL",
    )
    compile_database = json.loads(
        (context.run_root_path / "compile_commands.json").read_text(encoding="utf-8")
    )
    expected = str(Path(compile_database[0]["arguments"][0]).resolve())
    require(rows == [(expected,)], f"unexpected stored driver: {rows}")


@when(
    "the real extraction command indexes the external-template-specialization fixture"
)
def when_extract_indexes_external_template_specialization(
    context: FactsToolContext, external_template_specialization_source: Path
) -> None:
    extract_fixture(context, external_template_specialization_source)


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


@then("the specialization points to a lightweight external std::hash primary")
def then_specialization_target_is_lightweight_external(
    context: FactsToolContext,
) -> None:
    relation = query(
        context.facts_database_path,
        "SELECT source.qualified_name,source.is_external,source.is_definition,"
        "destination.qualified_name,destination.is_external,"
        "destination.is_definition FROM relation r "
        "JOIN symbol source ON source.id=r.source_id "
        "JOIN symbol destination ON destination.id=r.destination_id "
        "WHERE r.kind=4 "
        "AND source.qualified_name='std::hash' "
        "AND destination.qualified_name='std::hash'",
    )
    require(
        relation == [("std::hash", 0, 1, "std::hash", 1, 0)],
        f"unexpected external specialization target: {relation}",
    )
