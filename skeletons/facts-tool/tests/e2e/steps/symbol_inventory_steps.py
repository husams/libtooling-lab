from __future__ import annotations

from pytest_bdd import then
from support.database import parameters_by_function, query, require
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("the symbol inventory includes")
def then_symbol_inventory_includes(context: FactsToolContext, datatable: Table) -> None:
    expected = {
        (int(row["node"]), row["qualified_name"]) for row in table_records(datatable)
    }
    actual = set(
        query(
            context.facts_database_path,
            "SELECT node,qualified_name FROM symbol",
        )
    )
    require(
        expected <= actual,
        f"symbol inventory is missing: {expected - actual}",
    )


@then("the defined functions include")
def then_defined_functions_include(context: FactsToolContext, datatable: Table) -> None:
    expected = {row["qualified_name"] for row in table_records(datatable)}
    actual = {
        name
        for (name,) in query(
            context.facts_database_path,
            "SELECT s.qualified_name FROM definition d "
            "JOIN symbol s ON s.id=d.symbol_id",
        )
    }
    require(
        expected <= actual,
        f"missing function definitions: {expected - actual}",
    )


@then("the parameters for e2e::transform are")
def then_transform_parameters_are(context: FactsToolContext, datatable: Table) -> None:
    expected = [(int(row["position"]), row["name"]) for row in table_records(datatable)]
    actual = [
        (position, name)
        for position, name, _, _ in parameters_by_function(context.facts_database_path)[
            "e2e::transform"
        ]
    ]
    require(actual == expected, f"unexpected transform parameters: {actual}")


@then("the parameters for e2e::headerHelper are")
def then_header_helper_parameters_are(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = [
        (
            int(row["position"]),
            row["name"],
            int(row["has_default"]),
        )
        for row in table_records(datatable)
    ]
    actual = [
        (position, name, has_default)
        for position, name, _, has_default in parameters_by_function(
            context.facts_database_path
        )["e2e::headerHelper"]
    ]
    require(actual == expected, f"unexpected headerHelper parameters: {actual}")


@then("the facts database has no foreign-key violations")
def then_database_has_no_foreign_key_violations(context: FactsToolContext) -> None:
    require(
        not query(context.facts_database_path, "PRAGMA foreign_key_check"),
        "facts database has broken typed or relation references",
    )


@then("every relation references captured source and destination symbols")
def then_relations_reference_captured_symbols(context: FactsToolContext) -> None:
    require(
        not query(
            context.facts_database_path,
            "SELECT r.source_id,r.destination_id FROM relation r "
            "LEFT JOIN symbol s ON s.id=r.source_id "
            "LEFT JOIN symbol d ON d.id=r.destination_id "
            "WHERE s.id IS NULL OR d.id IS NULL",
        ),
        "relation rows must reference captured symbols",
    )
