from __future__ import annotations

from pytest_bdd import then
from support.database import query, require
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("the persisted enumeration facts include")
def then_persisted_enumeration_facts_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (
            row["qualified_name"],
            int(row["node"]),
            int(row["is_definition"]),
            int(row["is_scoped"]),
            int(row["has_fixed_underlying_type"]),
        )
        for row in table_records(datatable)
    }
    names = tuple(row[0] for row in expected)
    placeholders = ",".join("?" for _ in names)
    actual = set(
        query(
            context.facts_database_path,
            "SELECT s.qualified_name,s.node,s.is_definition,e.is_scoped,"
            "e.has_fixed_underlying_type FROM enumeration e "
            "JOIN symbol s ON s.id=e.symbol_id "
            f"WHERE s.qualified_name IN ({placeholders}) "
            "AND (e.underlying_type >> 32)=0 AND e.underlying_type<>0",
            names,
        )
    )
    require(actual == expected, f"unexpected enumeration facts: {actual}")


@then("the persisted enumerator facts include")
def then_persisted_enumerator_facts_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (
            row["qualified_name"],
            int(row["node"]),
            int(row["is_definition"]),
            row["value"],
            row["initializer_expression"],
        )
        for row in table_records(datatable)
    }
    names = tuple(row[0] for row in expected)
    placeholders = ",".join("?" for _ in names)
    actual = set(
        query(
            context.facts_database_path,
            "SELECT s.qualified_name,s.node,s.is_definition,e.value,"
            "e.initializer_expression FROM enumerator e "
            "JOIN symbol s ON s.id=e.symbol_id "
            f"WHERE s.qualified_name IN ({placeholders})",
            names,
        )
    )
    require(actual == expected, f"unexpected enumerator facts: {actual}")


@then("the persisted enum ownership relations include")
def then_persisted_enum_ownership_relations_include(
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
            "WHERE r.kind=3 AND source.qualified_name='e2e::Mode'",
        )
    )
    require(expected <= actual, f"missing enum ownership: {expected - actual}")
