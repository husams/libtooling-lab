from __future__ import annotations

import subprocess

from pytest_bdd import given, when
from support.database import require
from support.forced_include import ForcedIncludeFixture, fixture
from support.scenario import FactsToolContext


@given("a temporary forced-include fixture from the B-028 note")
def given_forced_fixture(context: FactsToolContext) -> None:
    context.forced_include = ForcedIncludeFixture.create(context, False)


@given("a temporary forced-include fixture with a system-search header")
def given_system_fixture(context: FactsToolContext) -> None:
    context.forced_include = ForcedIncludeFixture.create(context, True, True)


@given("the compiler control for the standard forced header succeeds")
def given_clang_control(context: FactsToolContext) -> None:
    state = fixture(context)
    result = subprocess.run(
        [str(context.compiler), "-std=c++17", "-include", "optional",
         "-fsyntax-only", str(state.source)], capture_output=True, text=True,
        check=False)
    require(result.returncode == 0, result.stdout + result.stderr)


@given("the compiler control for the system-search forced header succeeds")
def given_system_clang_control(context: FactsToolContext) -> None:
    state = fixture(context)
    result = subprocess.run(
        [str(context.compiler), "-std=c++17", "-isystem", str(state.root / "system"),
         "-include", "forced.hpp", "-fsyntax-only", str(state.source)],
        capture_output=True, text=True, check=False)
    require(result.returncode == 0, result.stdout + result.stderr)


@when("the real facts-tool imports the fixed command with the standard forced header")
def when_fixed_import(context: FactsToolContext) -> None:
    state = fixture(context)
    state.run_fixed_import(context, ["-std=c++17", "-include", "optional"],
                           state.root / "fixed.sqlite")


@when("the real facts-tool imports the fixed command with the system-search forced header")
def when_system_fixed_import(context: FactsToolContext) -> None:
    state = fixture(context)
    state.run_fixed_import(
        context, ["-std=c++17", "-isystem", "system", "-include", "forced.hpp"],
        state.root / "system-fixed.sqlite")


@when("the real facts-tool imports the compilation database with the standard forced header")
def when_database_import(context: FactsToolContext) -> None:
    state = fixture(context)
    state.write_compilation_database(context, ["-std=c++17", "-include", "optional"])
    state.run_database_import(context, state.root / "database.sqlite")


@when("the real facts-tool imports the compilation database with the system-search forced header")
def when_system_database_import(context: FactsToolContext) -> None:
    state = fixture(context)
    state.write_compilation_database(
        context, ["-std=c++17", "-isystem", "system", "-include", "forced.hpp"])
    state.run_database_import(context, state.root / "system-database.sqlite")


@when("the real facts-tool repeats that import with fresh temporary storage")
def when_repeat_import(context: FactsToolContext) -> None:
    state = fixture(context)
    state.run_database_import(context, state.root / "repeat.sqlite")


@when("the real facts-tool extracts using the stored forced header")
def when_stored_extraction(context: FactsToolContext) -> None:
    fixture(context).extract(context)
