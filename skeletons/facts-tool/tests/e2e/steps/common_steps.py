from __future__ import annotations

from pytest_bdd import given, when
from support.scenario import FactsToolContext


@given("realistic shared-header declarations, definitions, parameters, and relations")
def given_realistic_multifile_cpp(context: FactsToolContext) -> None:
    context.prepare()


@when("the real facts-tool indexes both translation units using compile_commands.json")
def when_facts_tool_indexes_translation_units(context: FactsToolContext) -> None:
    context.extract()
