from __future__ import annotations

import json
from pathlib import Path

from pytest_bdd import given, then, when
from steps.external_target_steps import extract_fixture, prepare_compile_database
from support.database import query, require
from support.scenario import FactsToolContext

ALIGNED_DELETE_USR = "c:@F@operator delete#*v#l#$@N@std@E@align_val_t#"
FIXTURES = ("b027_external_callee.cpp", "b027_external_callee_second.cpp")


@given(
    "a B-027 compile database with one runtime-callee source",
    target_fixture="b027_sources",
)
def given_runtime_callee_fixture(context: FactsToolContext) -> tuple[Path, ...]:
    source = prepare_compile_database(context, FIXTURES[0], "b027-runtime")
    return (source,)


@given(
    "a B-027 compile database with both runtime-callee sources",
    target_fixture="b027_sources",
)
def given_both_runtime_callee_fixtures(
    context: FactsToolContext,
) -> tuple[Path, ...]:
    context.prepare()
    sources = tuple(
        (context.fixture_root / name).resolve(strict=True) for name in FIXTURES
    )
    context.facts_database = context.run_root_path / "b027-runtime.sqlite"
    context.files_database = context.run_root_path / "b027-runtime-project.sqlite"
    commands = [
        {
            "directory": str(context.fixture_root),
            "file": str(source),
            "arguments": [str(context.compiler), "-std=c++23", "-c", str(source)],
        }
        for source in sources
    ]
    (context.run_root_path / "compile_commands.json").write_text(
        json.dumps(commands, indent=2) + "\n", encoding="utf-8"
    )
    return sources


@when("B-027 extraction runs for the runtime-callee source")
@when("B-027 extraction runs for both runtime-callee sources")
def when_extracts_runtime_callee(
    context: FactsToolContext, b027_sources: tuple[Path, ...]
) -> None:
    for source in b027_sources:
        extract_fixture(context, source)


@then("the B-027 extraction commits without invalid-USR diagnostics")
def then_runtime_extraction_commits(context: FactsToolContext) -> None:
    require(context.last_returncode == 0, context.last_output)
    require("invalid USR" not in context.last_output, context.last_output)
    require("rollback output transaction" not in context.last_output, context.last_output)


@then("the aligned runtime delete is one lightweight external call target")
def then_aligned_delete_is_canonical(context: FactsToolContext) -> None:
    symbols = query(
        context.facts_database_path,
        "SELECT COUNT(*),MIN(is_external) FROM symbol WHERE usr=?",
        (ALIGNED_DELETE_USR,),
    )
    calls = query(
        context.facts_database_path,
        "SELECT source.qualified_name,COUNT(*) FROM relation r "
        "JOIN symbol source ON source.id=r.source_id "
        "JOIN symbol target ON target.id=r.destination_id "
        "WHERE target.usr=? AND r.kind=1 GROUP BY source.qualified_name",
        (ALIGNED_DELETE_USR,),
    )
    require(symbols == [(1, 1)], f"unexpected aligned delete symbol: {symbols}")
    require(calls and all(count == 1 for _, count in calls), f"bad calls: {calls}")
