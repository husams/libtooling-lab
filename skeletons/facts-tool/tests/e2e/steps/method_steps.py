from __future__ import annotations

from pytest_bdd import then
from support.database import query, require
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("the persisted method symbol fields include")
def then_persisted_method_symbol_fields_include(
    context: FactsToolContext, datatable: Table
) -> None:
    fields = (
        "node",
        "is_definition",
        "is_inline",
        "is_virtual",
        "is_pure",
        "is_override",
        "is_defaulted",
        "is_deleted",
    )
    expected = {
        (row["qualified_name"], *(int(row[field]) for field in fields))
        for row in table_records(datatable)
    }
    names = tuple(row[0] for row in expected)
    placeholders = ",".join("?" for _ in names)
    actual = set(
        query(
            context.facts_database_path,
            "SELECT qualified_name,node,is_definition,is_inline,is_virtual,"
            "is_pure,is_override,is_defaulted,is_deleted FROM symbol "
            f"WHERE qualified_name IN ({placeholders})",
            names,
        )
    )
    require(actual == expected, f"unexpected persisted method fields: {actual}")


@then("the persisted method ownership relations include")
def then_persisted_method_ownership_relations_include(
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
            "WHERE r.kind=9",
        )
    )
    require(expected <= actual, f"missing method ownership relations: {expected}")
