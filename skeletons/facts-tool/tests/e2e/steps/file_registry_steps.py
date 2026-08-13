from __future__ import annotations

from pathlib import Path

from support.bdd import Table, table_records, then
from support.database import file_snapshot, query, require
from support.scenario import FactsToolContext


@then("the file registry contains these canonical fixture paths")
def then_registry_contains_canonical_fixture_paths(
    context: FactsToolContext, table: Table
) -> None:
    files = file_snapshot(context.files_database_path)
    expected_paths = {
        str((context.fixture_root / row["fixture"]).resolve(strict=True))
        for row in table_records(table)
    }
    actual_paths = {path for _, path in files}
    require(actual_paths == expected_paths, f"unexpected file registry: {files}")
    require(
        all(
            Path(path).is_absolute() and Path(path).resolve(strict=True) == Path(path)
            for _, path in files
        ),
        f"non-canonical source path: {files}",
    )


@then("every registered FileId is greater than zero")
def then_registered_file_ids_are_nonzero(context: FactsToolContext) -> None:
    files = file_snapshot(context.files_database_path)
    require(all(file_id > 0 for file_id, _ in files), "FileId 0 is reserved")


@then("every captured symbol uses a registered nonzero FileId")
def then_symbols_use_registered_file_ids(context: FactsToolContext) -> None:
    imported_ids = {
        file_id for file_id, _ in file_snapshot(context.files_database_path)
    }
    symbol_file_ids = {
        file_id
        for (file_id,) in query(
            context.facts_database_path,
            "SELECT DISTINCT file_id FROM symbol",
        )
    }
    require(0 not in symbol_file_ids, "captured symbols must not use builtin FileId 0")
    require(
        symbol_file_ids <= imported_ids,
        f"symbols reference non-preimported FileIds: {symbol_file_ids - imported_ids}",
    )
