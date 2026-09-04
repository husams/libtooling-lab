from __future__ import annotations

import subprocess

from pytest_bdd import given, parsers, then, when
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


@given("a temporary forced-include fixture with a system-search header")
def given_system_fixture(context: FactsToolContext) -> None:
    context.forced_include = ForcedIncludeFixture.create(context, True, True)


@given("the compiler control for the standard forced header succeeds")
def given_clang_control(context: FactsToolContext) -> None:
    state = fixture(context)
    result = subprocess.run([str(context.compiler), "-std=c++17", "-include", "optional",
                             "-fsyntax-only", str(state.source)],
                            capture_output=True, text=True, check=False)
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
        context,
        ["-std=c++17", "-isystem", "system", "-include", "forced.hpp"],
        state.root / "system-fixed.sqlite",
    )


@when("the real facts-tool imports the compilation database with the standard forced header")
def when_database_import(context: FactsToolContext) -> None:
    state = fixture(context)
    state.write_compilation_database(context, ["-std=c++17", "-include", "optional"])
    state.run_database_import(context, state.root / "database.sqlite")


@when("the real facts-tool imports the compilation database with the system-search forced header")
def when_system_database_import(context: FactsToolContext) -> None:
    state = fixture(context)
    state.write_compilation_database(
        context, ["-std=c++17", "-isystem", "system", "-include", "forced.hpp"]
    )
    state.run_database_import(context, state.root / "system-database.sqlite")


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


@when("the real facts-tool imports the imacros forced header path")
def when_imacros_path(context: FactsToolContext) -> None:
    state = fixture(context)
    state.write_compilation_database(context, ["-std=c++17", "-imacros",
                                               "include/forced.hpp"])
    state.run_database_import(context, state.root / "imacros.sqlite")


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


@then(parsers.parse('the stored forced-include option is "{expected}"'))
def then_stored_option(context: FactsToolContext, expected: str) -> None:
    options = fixture(context).stored_options()
    include = options.index("-include")
    require(options[include + 1] == expected, options)


@then("the stored forced-include option is the unchanged absolute header path")
def then_absolute_stored_option(context: FactsToolContext) -> None:
    options = fixture(context).stored_options()
    include = options.index("-include")
    require(options[include + 1] == str(fixture(context).header), options)


@then("the stored imacros option is unchanged")
def then_imacros_option(context: FactsToolContext) -> None:
    options = fixture(context).stored_options()
    imacros = options.index("-imacros")
    require(options[imacros + 1] == "include/forced.hpp", options)


@then("extracting with the stored forced header succeeds")
def then_stored_extraction_succeeds(context: FactsToolContext) -> None:
    fixture(context).extract(context)
    require(context.last_returncode == 0, context.last_output)


@then("the forced-include import fails with an actionable diagnostic")
def then_missing_fails(context: FactsToolContext) -> None:
    require(context.last_returncode not in (None, 0), context.last_output)
    require("missing.hpp" in context.last_output and "cannot enumerate included files" in context.last_output,
            context.last_output)
