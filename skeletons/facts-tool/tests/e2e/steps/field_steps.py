from __future__ import annotations

from pytest_bdd import then
from support.database import query, require
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("the persisted field symbol fields include")
def then_persisted_field_symbol_fields_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (
            row["qualified_name"],
            int(row["node"]),
            int(row["is_definition"]),
            row["access"],
        )
        for row in table_records(datatable)
    }
    names = tuple(row[0] for row in expected)
    placeholders = ",".join("?" for _ in names)
    actual = set(
        query(
            context.facts_database_path,
            "SELECT qualified_name,node,is_definition,access FROM symbol "
            f"WHERE qualified_name IN ({placeholders})",
            names,
        )
    )
    require(actual == expected, f"unexpected persisted field fields: {actual}")


@then("the persisted field ownership relations include")
def then_persisted_field_ownership_relations_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (row["source"], row["destination"], int(row["kind"]))
        for row in table_records(datatable)
    }
    actual = set(
        query(
            context.facts_database_path,
            "SELECT source.qualified_name,destination.qualified_name,r.kind "
            "FROM relation r "
            "JOIN symbol source ON source.id=r.source_id "
            "JOIN symbol destination ON destination.id=r.destination_id "
            "WHERE r.kind=8",
        )
    )
    require(expected <= actual, f"missing field ownership relations: {expected}")
