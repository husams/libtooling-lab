from __future__ import annotations

from pytest_bdd import then
from support.database import query, require
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("the persisted parameter defaults include")
def then_persisted_parameter_defaults_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (
            row["qualified_name"],
            int(row["position"]),
            row["name"],
            row["expression"],
            row["evaluated_kind"],
            row["evaluated_value"] or None,
        )
        for row in table_records(datatable)
    }
    names = tuple({row[0] for row in expected})
    placeholders = ",".join("?" for _ in names)
    actual = set(
        query(
            context.facts_database_path,
            "SELECT s.qualified_name,p.position,p.name,d.expression,"
            "d.evaluated_kind,d.evaluated_value FROM parameter_default d "
            "JOIN parameter p ON p.symbol_id=d.symbol_id "
            "AND p.position=d.position "
            "JOIN symbol s ON s.id=d.symbol_id "
            f"WHERE s.qualified_name IN ({placeholders})",
            names,
        )
    )
    require(actual == expected, f"unexpected parameter defaults: {actual}")
