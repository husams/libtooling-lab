from __future__ import annotations

from pytest_bdd import then, when
from support.database import require, symbol_snapshot
from support.scenario import FactsToolContext


@when("the real facts-tool reruns using a stored component-label include path")
def when_facts_tool_uses_labeled_stored_options(context: FactsToolContext) -> None:
    context.rerun_from_labeled_stored_compile_options()


@when("the real facts-tool reruns with malformed stored options while JSON remains")
def when_json_takes_precedence(context: FactsToolContext) -> None:
    context.rerun_with_json_over_malformed_stored_options()


@when("the real facts-tool runs using malformed stored options without JSON")
def when_stored_options_are_malformed(context: FactsToolContext) -> None:
    context.run_with_malformed_stored_options()


@when("the real facts-tool runs without a stored command for two.cpp")
def when_a_stored_command_is_missing(context: FactsToolContext) -> None:
    context.run_with_missing_stored_command("two.cpp")


@then("the stored-option extraction matches the JSON extraction")
def then_stored_facts_match_json(context: FactsToolContext) -> None:
    require(
        symbol_snapshot(context.facts_database_path) == context.initial_symbols,
        "stored compile options produced different facts than compile_commands.json",
    )


@then("the facts-tool run succeeds")
def then_facts_tool_succeeds(context: FactsToolContext) -> None:
    require(
        context.last_returncode == 0,
        f"facts-tool exited with {context.last_returncode}:\n{context.last_output}",
    )


@then("the facts-tool run fails")
def then_facts_tool_fails(context: FactsToolContext) -> None:
    require(
        context.last_returncode not in (None, 0),
        f"facts-tool unexpectedly succeeded:\n{context.last_output}",
    )


@then("the diagnostic mentions malformed compile options")
def then_diagnostic_mentions_malformed_options(context: FactsToolContext) -> None:
    require(
        "compile_options is not a JSON array" in context.last_output,
        f"missing malformed-options diagnostic:\n{context.last_output}",
    )


@then("the diagnostic mentions the missing compile command for two.cpp")
def then_diagnostic_mentions_missing_command(context: FactsToolContext) -> None:
    require(
        "two.cpp" in context.last_output
        and "compile command" in context.last_output.lower(),
        f"missing compile-command diagnostic:\n{context.last_output}",
    )
