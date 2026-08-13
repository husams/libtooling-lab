from __future__ import annotations

from support.bdd import step
from support.database import file_snapshot, require, scalar, symbol_snapshot
from support.scenario import FactsToolContext


@step("indexing is repeated and two extractor processes run concurrently")
def when_indexing_is_repeated_and_concurrent(context: FactsToolContext) -> None:
    context.run_tool()
    require(
        file_snapshot(context.files_database_path) == context.initial_files,
        "FileIds changed after rerun",
    )
    require(
        symbol_snapshot(context.facts_database_path) == context.initial_symbols,
        "SymbolIds changed after rerun",
    )

    context.run_concurrently()
    require(
        file_snapshot(context.files_database_path) == context.initial_files,
        "FileIds changed after concurrency",
    )
    require(
        symbol_snapshot(context.facts_database_path) == context.initial_symbols,
        "SymbolIds or logical symbols changed after concurrency",
    )
    require(
        scalar(context.facts_database_path, "SELECT COUNT(*) FROM symbol")
        == scalar(
            context.facts_database_path,
            "SELECT COUNT(DISTINCT id) FROM symbol",
        ),
        "duplicate SymbolIds were stored",
    )
    context.stability_checked = True


@step("FileIds and SymbolIds remain stable without duplicate logical symbols")
def then_identifiers_remain_stable(context: FactsToolContext) -> None:
    require(context.stability_checked, "rerun and concurrency checks did not run")
