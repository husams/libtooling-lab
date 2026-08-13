from __future__ import annotations

from pytest_bdd import then, when
from support.database import file_snapshot, require, scalar, symbol_snapshot
from support.scenario import FactsToolContext


def require_initial_snapshots(context: FactsToolContext) -> None:
    require(
        file_snapshot(context.files_database_path) == context.initial_files,
        "FileIds changed after rerun",
    )
    require(
        symbol_snapshot(context.facts_database_path) == context.initial_symbols,
        "SymbolIds changed after rerun",
    )


@when("indexing is repeated once")
def when_indexing_is_repeated_once(context: FactsToolContext) -> None:
    context.run_tool()


@then("FileIds and SymbolIds match the initial extraction")
def then_identifiers_match_initial_extraction(context: FactsToolContext) -> None:
    require_initial_snapshots(context)


@when("two extractor processes run concurrently")
def when_two_extractors_run_concurrently(context: FactsToolContext) -> None:
    context.run_concurrently()


@then("FileIds and SymbolIds still match the initial extraction")
def then_identifiers_still_match_initial_extraction(
    context: FactsToolContext,
) -> None:
    require_initial_snapshots(context)


@then("no duplicate SymbolIds are stored")
def then_no_duplicate_symbol_ids_are_stored(context: FactsToolContext) -> None:
    require(
        scalar(context.facts_database_path, "SELECT COUNT(*) FROM symbol")
        == scalar(
            context.facts_database_path,
            "SELECT COUNT(DISTINCT id) FROM symbol",
        ),
        "duplicate SymbolIds were stored",
    )
