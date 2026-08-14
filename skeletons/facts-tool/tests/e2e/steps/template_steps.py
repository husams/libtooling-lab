from __future__ import annotations

from pytest_bdd import then
from support.database import query, require
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("the template symbols include")
def then_template_symbols_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (row["qualified_name"], int(row["node"]), int(row["kind"]))
        for row in table_records(datatable)
    }
    actual = set(
        query(
            context.facts_database_path,
            "SELECT qualified_name,node,kind FROM symbol WHERE id IN "
            "(SELECT DISTINCT symbol_id FROM template_argument)",
        )
    )
    require(expected <= actual, f"missing template symbols: {expected - actual}")


@then("the declared template arguments are")
def then_declared_template_arguments_are(
    context: FactsToolContext, datatable: Table
) -> None:
    fields = (
        "position",
        "is_parameter_pack",
        "is_non_type",
        "is_template_template",
    )
    expected = {
        (
            row["qualified_name"],
            int(row[fields[0]]),
            row["name"],
            *(int(row[field]) for field in fields[1:]),
        )
        for row in table_records(datatable)
    }
    names = tuple({row[0] for row in expected})
    placeholders = ",".join("?" for _ in names)
    actual = set(
        query(
            context.facts_database_path,
            "SELECT s.qualified_name,a.position,a.name,a.is_parameter_pack,"
            "a.is_non_type,a.is_template_template FROM template_argument a "
            "JOIN symbol s ON s.id=a.symbol_id "
            f"WHERE s.qualified_name IN ({placeholders})",
            names,
        )
    )
    require(
        actual == expected,
        f"unexpected declared template arguments: {actual}",
    )


@then("every non-type template argument has a predefined type ID")
def then_non_type_arguments_have_predefined_type_ids(
    context: FactsToolContext,
) -> None:
    type_ids = [
        type_id
        for (type_id,) in query(
            context.facts_database_path,
            "SELECT type_id FROM template_argument WHERE is_non_type=1",
        )
    ]
    require(type_ids, "expected at least one non-type template argument")
    require(
        all(type_id > 0 and type_id >> 32 == 0 for type_id in type_ids),
        f"non-type template arguments must use predefined type IDs: {type_ids}",
    )
