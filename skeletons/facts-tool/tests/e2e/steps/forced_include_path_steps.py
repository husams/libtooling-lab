from __future__ import annotations

from pytest_bdd import given, when
from support.forced_include import ForcedIncludeFixture, fixture
from support.scenario import FactsToolContext


@given("a temporary forced-include fixture with an explicit local header")
def given_explicit_fixture(context: FactsToolContext) -> None:
    context.forced_include = ForcedIncludeFixture.create(context, True)


@when("the real facts-tool imports the relative forced header path")
def when_relative_path(context: FactsToolContext) -> None:
    state = fixture(context)
    state.write_compilation_database(
        context, ["-std=c++17", "-include", "include/forced.hpp"])
    state.run_database_import(context, state.root / "relative.sqlite")


@when("the real facts-tool imports the absolute forced header path")
def when_absolute_path(context: FactsToolContext) -> None:
    state = fixture(context)
    state.run_fixed_import(context, ["-std=c++17", "-include", str(state.header)],
                           state.root / "absolute.sqlite")


@when("the real facts-tool imports the imacros forced header path")
def when_imacros_path(context: FactsToolContext) -> None:
    state = fixture(context)
    state.write_compilation_database(
        context, ["-std=c++17", "-imacros", "include/forced.hpp"])
    state.run_database_import(context, state.root / "imacros.sqlite")


@when("the real facts-tool imports a missing forced header")
def when_missing_header(context: FactsToolContext) -> None:
    state = fixture(context)
    state.run_fixed_import(context, ["-std=c++17", "-include", "missing.hpp"],
                           state.root / "missing.sqlite")


@when("the real facts-tool imports one shell-style forced-header argument")
def when_single_extra_arg(context: FactsToolContext) -> None:
    state = fixture(context)
    state.run_fixed_import(context, ["-std=c++17", "-include optional"],
                           state.root / "single-extra-arg.sqlite")


@when("the real facts-tool imports a quoted extra argument with a space")
def when_quoted_extra_arg(context: FactsToolContext) -> None:
    state = fixture(context)
    state.run_fixed_import(
        context, ["-std=c++17", "-include optional", '-DMSG="hello world"'],
        state.root / "quoted-extra-arg.sqlite")
