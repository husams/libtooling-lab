from __future__ import annotations

from pytest_bdd import then
from support.database import query, require
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("the persisted variable initializers include")
def then_persisted_variable_initializers_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (
            row["qualified_name"],
            row["expression"],
            row["evaluated_kind"],
            row["evaluated_value"] or None,
        )
        for row in table_records(datatable)
    }
    names = tuple(row[0] for row in expected)
    placeholders = ",".join("?" for _ in names)
    actual = set(
        query(
            context.facts_database_path,
            "SELECT s.qualified_name,i.expression,i.evaluated_kind,"
            "i.evaluated_value FROM variable_initializer i "
            "JOIN symbol s ON s.id=i.symbol_id "
            f"WHERE s.qualified_name IN ({placeholders})",
            names,
        )
    )
    require(actual == expected, f"unexpected variable initializers: {actual}")


@then("the persisted variable symbol fields include")
def then_persisted_variable_symbol_fields_include(
    context: FactsToolContext, datatable: Table
) -> None:
    fields = (
        "node",
        "is_definition",
        "is_static",
        "is_const",
        "is_inline",
        "has_internal_linkage",
        "has_extern_storage",
    )
    expected = {
        (
            row["qualified_name"],
            *(int(row[field]) for field in fields),
            row["constant_evaluation"],
        )
        for row in table_records(datatable)
    }
    names = tuple(row[0] for row in expected)
    placeholders = ",".join("?" for _ in names)
    actual = set(
        query(
            context.facts_database_path,
            "SELECT qualified_name,node,is_definition,is_static,is_const,"
            "is_inline,has_internal_linkage,has_extern_storage,"
            "constant_evaluation FROM symbol "
            f"WHERE qualified_name IN ({placeholders})",
            names,
        )
    )
    require(actual == expected, f"unexpected variable symbol fields: {actual}")


@then("the persisted value relations include")
def then_persisted_value_relations_include(
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
            "WHERE r.kind IN (8,20)",
        )
    )
    require(expected <= actual, f"missing value relations: {expected - actual}")


@then("block-scope variables are not persisted")
def then_block_scope_variables_are_not_persisted(
    context: FactsToolContext, datatable: Table
) -> None:
    names = tuple(row["qualified_name"] for row in table_records(datatable))
    placeholders = ",".join("?" for _ in names)
    actual = query(
        context.facts_database_path,
        "SELECT qualified_name FROM symbol "
        f"WHERE qualified_name IN ({placeholders})",
        names,
    )
    require(not actual, f"unexpected block-scope variables: {actual}")
