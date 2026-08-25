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


@given("the project configuration uses an outdated file registry")
def given_outdated_file_registry(context: FactsToolContext) -> None:
    context.outdate_file_registry()


@when("the real facts-tool extracts one translation unit for the registry check")
def when_facts_tool_extracts_for_registry_check(context: FactsToolContext) -> None:
    context.extract_translation_unit()


@then("extraction fails with an outdated-registry diagnostic")
def then_extraction_reports_outdated_registry(context: FactsToolContext) -> None:
    require(
        context.last_returncode == 1,
        f"expected a reported failure, got {context.last_returncode}:"
        f"\n{context.last_output}",
    )
    require(
        "outdated file registry" in context.last_output,
        f"missing outdated-registry diagnostic:\n{context.last_output}",
    )


@given("a compilation database whose translation unit includes a missing header")
def given_unpreprocessable_translation_unit(context: FactsToolContext) -> None:
    context.prepare()


@when("the real facts-tool imports that project")
def when_facts_tool_imports_unpreprocessable_project(
    context: FactsToolContext,
) -> None:
    context.import_with_unpreprocessable_source()


@then("import fails with an incomplete-registry diagnostic")
def then_import_reports_incomplete_registry(context: FactsToolContext) -> None:
    require(
        context.last_returncode == 1,
        f"expected import to fail, got {context.last_returncode}:"
        f"\n{context.last_output}",
    )
    require(
        "cannot enumerate included files" in context.last_output,
        f"missing incomplete-registry diagnostic:\n{context.last_output}",
    )


@given("a compile command that includes a precompiled header")
def given_precompiled_header_command(context: FactsToolContext) -> None:
    context.prepare()


@when("the real facts-tool imports before the prefix header is compiled")
def when_import_precedes_the_prefix_header(context: FactsToolContext) -> None:
    context.import_before_the_prefix_header_is_compiled()


@when("the real facts-tool imports and extracts with the prefix header compiled")
def when_import_and_extract_with_prefix_header(context: FactsToolContext) -> None:
    context.import_and_extract_with_a_compiled_prefix_header()


@then("the facts database contains the precompiled-header declarations")
def then_facts_contain_precompiled_header_declarations(
    context: FactsToolContext,
) -> None:
    names = {
        name
        for (name,) in query(
            context.facts_database_path, "SELECT qualified_name FROM symbol"
        )
    }
    expected = {"consume", "pch::make", "pch::Holder", "std::expected"}
    require(
        expected <= names,
        f"declarations deserialized from the prefix header are missing: "
        f"{expected - names}",
    )
