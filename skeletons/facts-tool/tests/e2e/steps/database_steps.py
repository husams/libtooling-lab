from __future__ import annotations

from support.bdd import step
from support.database import require, table_names
from support.scenario import FactsToolContext


@step("file identity and captured facts remain in separate databases")
def then_files_and_facts_databases_are_separate(context: FactsToolContext) -> None:
    facts_tables = table_names(context.facts_database_path)
    files_tables = table_names(context.files_database_path)
    require(
        "file" not in facts_tables,
        f"file registry leaked into facts database: {facts_tables}",
    )
    require(
        files_tables == {"file"},
        f"facts leaked into files database: {files_tables}",
    )
    require(
        {"symbol", "symbol_allocator", "definition", "parameter", "relation"}
        <= facts_tables,
        f"facts schema is incomplete: {facts_tables}",
    )
    require(
        context.facts_database_path != context.files_database_path,
        "database paths must differ",
    )
