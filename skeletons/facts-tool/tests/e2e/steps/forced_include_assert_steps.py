from __future__ import annotations

from pytest_bdd import parsers, then
from support.database import require
from support.forced_include import fixture
from support.scenario import FactsToolContext


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


@then("the stored options preserve the single forced-header argument")
def then_single_extra_arg(context: FactsToolContext) -> None:
    require(fixture(context).stored_options()[:3] ==
            ["-std=c++17", "-include", "optional"],
            fixture(context).stored_options())


@then("the stored quoted definition remains one token")
def then_quoted_extra_arg(context: FactsToolContext) -> None:
    require(fixture(context).stored_options()[:4] ==
            ["-std=c++17", "-include", "optional", "-DMSG=hello world"],
            fixture(context).stored_options())


@then("extracting with the stored forced header succeeds")
def then_stored_extraction_succeeds(context: FactsToolContext) -> None:
    fixture(context).extract(context)
    require(context.last_returncode == 0, context.last_output)


@then("the forced-include import fails with an actionable diagnostic")
def then_missing_fails(context: FactsToolContext) -> None:
    require(context.last_returncode not in (None, 0), context.last_output)
    require("missing.hpp" in context.last_output and
            "cannot enumerate included files" in context.last_output,
            context.last_output)
