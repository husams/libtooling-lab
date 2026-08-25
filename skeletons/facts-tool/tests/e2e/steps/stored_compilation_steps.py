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


@when(
    "the real facts-tool imports and extracts one.cpp "
    "while two.cpp keeps a missing include directory"
)
def when_unrelated_command_has_a_missing_include_root(
    context: FactsToolContext,
) -> None:
    context.run_with_unrelated_missing_include_root()


@when("the real facts-tool imports and extracts one.cpp with a missing include directory")
def when_selected_command_has_a_missing_include_root(
    context: FactsToolContext,
) -> None:
    context.run_with_missing_include_root_on_selected_source()


@when("the real facts-tool is invoked with the deprecated --files-out option")
def when_deprecated_files_out_is_used(context: FactsToolContext) -> None:
    context.run_with_deprecated_files_out_option()


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


@then("the diagnostic reports --files-out as an unknown option")
def then_diagnostic_rejects_files_out(context: FactsToolContext) -> None:
    require(
        "--files-out" in context.last_output
        and (
            "not expected" in context.last_output.lower()
            or "unrecognized" in context.last_output.lower()
        ),
        f"missing unknown-option diagnostic:\n{context.last_output}",
    )


@then("no diagnostic reports a file registration failure")
def then_no_file_registration_failure(context: FactsToolContext) -> None:
    combined = context.import_output + context.last_output
    require(
        "cannot register compilation files" not in combined,
        f"unexpected file registration failure:\n{combined}",
    )


@then("the import diagnostic reports the skipped include directory")
def then_import_diagnostic_reports_skipped_include_directory(
    context: FactsToolContext,
) -> None:
    require(
        f"skipping unavailable include directory '{context.missing_include_root}'"
        in context.import_output,
        f"missing skipped-include diagnostic:\n{context.import_output}",
    )
