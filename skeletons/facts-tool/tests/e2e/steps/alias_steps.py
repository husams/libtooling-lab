from __future__ import annotations

from pytest_bdd import then
from support.database import query, require, table_names
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("the alias symbols include")
def then_alias_symbols_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (row["qualified_name"], int(row["node"]))
        for row in table_records(datatable)
    }
    actual = set(
        query(
            context.facts_database_path,
            "SELECT qualified_name,node FROM symbol",
        )
    )
    require(expected <= actual, f"missing alias symbols: {expected - actual}")


@then("the alias relations include")
def then_alias_relations_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (row["source"], row["destination"])
        for row in table_records(datatable)
    }
    actual = set(
        query(
            context.facts_database_path,
            "SELECT DISTINCT source.qualified_name,destination.qualified_name "
            "FROM relation "
            "JOIN symbol source ON source.id=relation.source_id "
            "JOIN symbol destination ON destination.id=relation.destination_id "
            "WHERE relation.kind=19",
        )
    )
    require(expected <= actual, f"missing alias relations: {expected - actual}")


@then("the facts database excludes the legacy type alias table")
def then_no_legacy_type_alias_table(context: FactsToolContext) -> None:
    tables = table_names(context.facts_database_path)
    require("type_alias" not in tables, f"legacy type_alias table remains: {tables}")
