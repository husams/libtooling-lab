from __future__ import annotations

from pytest_bdd import then, when

from steps.matched_symbol_index_steps import find
from steps.native_matcher_workflow_steps import run
from support.database import require
from support.scenario import FactsToolContext


@when("matched-symbol lookup receives an empty name selector")
def empty_name(context: FactsToolContext) -> None:
    context.s026_empty_name = run([
        str(context.facts_tool), "symbol", "find", "--conf",
        str(context.files_database), "--name", "",
    ])


@then("the empty matched-symbol selector is rejected")
def empty_name_rejected(context: FactsToolContext) -> None:
    require(context.s026_empty_name.returncode == 2 and
            "--name must not be empty" in context.s026_empty_name.stderr,
            context.s026_empty_name.stdout + context.s026_empty_name.stderr)


@when("the matched index is cleared with an unknown positive file ID")
def clear_unknown_file(context: FactsToolContext) -> None:
    context.s026_before_unknown_clear = find(context, "--name", "targeted_match")["matches"]
    context.s026_unknown_clear = run([
        str(context.facts_tool), "symbol", "index", "clear", "--conf",
        str(context.files_database), "--file-id", "999999",
    ])


@then("the unknown clear succeeds without changing candidates")
def unknown_clear_noop(context: FactsToolContext) -> None:
    require(context.s026_unknown_clear.returncode == 0 and
            "Cleared 0 matched symbol candidate(s)" in context.s026_unknown_clear.stdout,
            context.s026_unknown_clear.stdout + context.s026_unknown_clear.stderr)
    require(find(context, "--name", "targeted_match")["matches"] ==
            context.s026_before_unknown_clear, "unknown clear changed candidates")
