from __future__ import annotations

from pytest_bdd import then
from support.database import query, require
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("the record symbols are")
def then_record_symbols_are(context: FactsToolContext, datatable: Table) -> None:
    rows = table_records(datatable)
    expected = {row["qualified_name"]: int(row["node"]) for row in rows}
    placeholders = ",".join("?" for _ in expected)
    actual = {
        qualified_name: node
        for qualified_name, node in query(
            context.facts_database_path,
            f"SELECT qualified_name,node FROM symbol "
            f"WHERE qualified_name IN ({placeholders})",
            tuple(expected),
        )
    }
    require(actual == expected, f"unexpected C++ record symbols: {actual}")


@then("the persisted record symbol fields include")
def then_persisted_record_symbol_fields_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (row["qualified_name"], int(row["is_definition"]))
        for row in table_records(datatable)
    }
    names = tuple(name for name, _ in expected)
    placeholders = ",".join("?" for _ in names)
    actual = set(
        query(
            context.facts_database_path,
            "SELECT qualified_name,is_definition FROM symbol "
            f"WHERE qualified_name IN ({placeholders})",
            names,
        )
    )
    require(actual == expected, f"unexpected persisted record fields: {actual}")
