from __future__ import annotations

from pathlib import Path

from support.bdd import step
from support.database import file_snapshot, query, require
from support.scenario import FactsToolContext


@step("every source path is canonical and every symbol uses a preimported FileId")
def then_paths_are_canonical_and_preimported(context: FactsToolContext) -> None:
    files = file_snapshot(context.files_database_path)
    expected_paths = {
        str(context.header),
        *(str(source) for source in context.sources),
    }
    actual_paths = {path for _, path in files}
    require(actual_paths == expected_paths, f"unexpected file registry: {files}")
    require(all(file_id > 0 for file_id, _ in files), "FileId 0 is reserved")
    require(
        all(
            Path(path).is_absolute() and Path(path).resolve(strict=True) == Path(path)
            for _, path in files
        ),
        f"non-canonical source path: {files}",
    )

    imported_ids = {file_id for file_id, _ in files}
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
