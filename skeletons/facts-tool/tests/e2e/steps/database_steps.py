from __future__ import annotations

from support.bdd import Table, table_records, then
from support.database import query, require, table_names
from support.scenario import FactsToolContext


@then("the facts database contains these tables")
def then_facts_database_contains_tables(
    context: FactsToolContext, table: Table
) -> None:
    expected = {row["table"] for row in table_records(table)}
    actual = table_names(context.facts_database_path)
    require(expected <= actual, f"facts schema is missing: {expected - actual}")


@then("the facts database excludes the file table")
def then_facts_database_excludes_file_table(context: FactsToolContext) -> None:
    facts_tables = table_names(context.facts_database_path)
    require(
        "file" not in facts_tables,
        f"file registry leaked into facts database: {facts_tables}",
    )


@then("no facts table stores opaque packed flags")
def then_no_facts_table_stores_packed_flags(context: FactsToolContext) -> None:
    tables = (
        "symbol",
        "parameter",
        "relation",
        "template_argument",
        "template_parameter",
    )
    packed_columns = {
        table
        for table in tables
        if any(
            name == "flags"
            for _, name, *_ in query(
                context.facts_database_path, f"PRAGMA table_info({table})"
            )
        )
    }
    require(not packed_columns, f"packed flags remain in: {packed_columns}")


@then("the files database contains only these tables")
def then_files_database_contains_only_tables(
    context: FactsToolContext, table: Table
) -> None:
    expected = {row["table"] for row in table_records(table)}
    actual = table_names(context.files_database_path)
    require(actual == expected, f"unexpected files database tables: {actual}")


@then("the facts and files databases use different paths")
def then_database_paths_are_different(context: FactsToolContext) -> None:
    require(
        context.facts_database_path != context.files_database_path,
        "database paths must differ",
    )
