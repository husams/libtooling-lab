from __future__ import annotations

from pytest_bdd import given, then, when
from support.database import query, require, scalar, table_names
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("the facts database contains these tables")
def then_facts_database_contains_tables(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {row["table"] for row in table_records(datatable)}
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
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {row["table"] for row in table_records(datatable)}
    actual = table_names(context.files_database_path)
    require(actual == expected, f"unexpected files database tables: {actual}")


@then("the facts and files databases use different paths")
def then_database_paths_are_different(context: FactsToolContext) -> None:
    require(
        context.facts_database_path != context.files_database_path,
        "database paths must differ",
    )


@given("a project configuration imported from a compilation database")
def given_imported_project_configuration(context: FactsToolContext) -> None:
    context.import_isolated_configuration()


@given("the project configuration database is read-only")
def given_read_only_project_configuration(context: FactsToolContext) -> None:
    context.make_configuration_read_only()


@when("the real facts-tool extracts one translation unit")
def when_facts_tool_extracts_one_translation_unit(
    context: FactsToolContext,
) -> None:
    context.extract_single_translation_unit()


@then("extraction succeeds")
def then_extraction_succeeds(context: FactsToolContext) -> None:
    require(
        context.last_returncode == 0,
        f"facts-tool exited with {context.last_returncode}:\n{context.last_output}",
    )
    require(
        "symbol(s) recorded" in context.last_output,
        f"missing extraction summary:\n{context.last_output}",
    )


@then("the facts database contains the extracted symbols")
def then_facts_database_contains_extracted_symbols(
    context: FactsToolContext,
) -> None:
    symbols = scalar(context.facts_database_path, "SELECT COUNT(*) FROM symbol")
    require(symbols > 0, "extraction recorded no symbols")


@then("the project configuration database is unchanged")
def then_project_configuration_is_unchanged(context: FactsToolContext) -> None:
    require(
        context.configuration_bytes is not None,
        "no project configuration snapshot was taken",
    )
    require(
        context.files_database_path.read_bytes() == context.configuration_bytes,
        "extraction modified the project configuration database",
    )
