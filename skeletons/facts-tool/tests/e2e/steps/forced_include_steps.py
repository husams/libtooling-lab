from __future__ import annotations

import subprocess

from pytest_bdd import given, then, when
from support.database import require
from support.forced_include import ForcedIncludeFixture
from support.scenario import FactsToolContext


def fixture(context: FactsToolContext) -> ForcedIncludeFixture:
    return context.forced_include


@given("a temporary forced-include fixture from the B-028 note")
def given_forced_fixture(context: FactsToolContext) -> None:
    context.forced_include = ForcedIncludeFixture.create(context, False)


@given("a temporary forced-include fixture with an explicit local header")
def given_explicit_fixture(context: FactsToolContext) -> None:
    context.forced_include = ForcedIncludeFixture.create(context, True)


@given("the compiler control for the standard forced header succeeds")
def given_clang_control(context: FactsToolContext) -> None:
    state = fixture(context)
    result = subprocess.run([str(context.compiler), "-std=c++17", "-include", "optional",
                             "-fsyntax-only", str(state.source)],
                            capture_output=True, text=True, check=False)
    require(result.returncode == 0, result.stdout + result.stderr)


@when("the real facts-tool imports the fixed command with the standard forced header")
def when_fixed_import(context: FactsToolContext) -> None:
    state = fixture(context)
    state.run_fixed_import(context, ["-std=c++17", "-include", "optional"],
                           state.root / "fixed.sqlite")


@when("the real facts-tool imports the compilation database with the standard forced header")
def when_database_import(context: FactsToolContext) -> None:
    state = fixture(context)
    state.write_compilation_database(context, ["-std=c++17", "-include", "optional"])
    state.run_database_import(context, state.root / "database.sqlite")


@when("the real facts-tool repeats that import with fresh temporary storage")
def when_repeat_import(context: FactsToolContext) -> None:
    state = fixture(context)
    state.run_database_import(context, state.root / "repeat.sqlite")


@when("the real facts-tool extracts using the stored forced header")
def when_stored_extraction(context: FactsToolContext) -> None:
    fixture(context).extract(context)


@when("the real facts-tool imports the relative forced header path")
def when_relative_path(context: FactsToolContext) -> None:
    state = fixture(context)
    state.write_compilation_database(context, ["-std=c++17", "-include", "include/forced.hpp"])
    state.run_database_import(context, state.root / "relative.sqlite")


@when("the real facts-tool imports the absolute forced header path")
def when_absolute_path(context: FactsToolContext) -> None:
    state = fixture(context)
    state.run_fixed_import(context, ["-std=c++17", "-include", str(state.header)],
                           state.root / "absolute.sqlite")


@when("the real facts-tool imports a missing forced header")
def when_missing_header(context: FactsToolContext) -> None:
    state = fixture(context)
    state.run_fixed_import(context, ["-std=c++17", "-include", "missing.hpp"],
                           state.root / "missing.sqlite")


@then("the forced-include import succeeds")
def then_import_succeeds(context: FactsToolContext) -> None:
    require(context.last_returncode == 0, context.last_output)


@then("the stored forced-include options preserve compiler order")
def then_options_preserve_order(context: FactsToolContext) -> None:
    options = fixture(context).stored_options()
    include = options.index("-include")
    require(options[include + 1] == "optional", options)
    require(options.index("-std=c++17") < include, options)


@then("the forced-include import fails with an actionable diagnostic")
def then_missing_fails(context: FactsToolContext) -> None:
    require(context.last_returncode not in (None, 0), context.last_output)
    require("missing.hpp" in context.last_output and "cannot enumerate included files" in context.last_output,
            context.last_output)
