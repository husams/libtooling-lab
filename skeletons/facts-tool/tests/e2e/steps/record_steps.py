from __future__ import annotations

from support.bdd import Table, table_records, then
from support.database import query, require
from support.scenario import FactsToolContext


@then("the record symbols are")
def then_record_symbols_are(context: FactsToolContext, table: Table) -> None:
    rows = table_records(table)
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


@then("the record definition states are")
def then_record_definition_states_are(context: FactsToolContext, table: Table) -> None:
    expected = {
        row["qualified_name"]: row["defined"] == "yes" for row in table_records(table)
    }
    defined = {
        name
        for (name,) in query(
            context.facts_database_path,
            "SELECT s.qualified_name FROM definition d "
            "JOIN symbol s ON s.id=d.symbol_id",
        )
    }
    actual = {name: name in defined for name in expected}
    require(actual == expected, f"unexpected record definition states: {actual}")
