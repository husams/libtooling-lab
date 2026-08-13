from __future__ import annotations

from support.bdd import Table, table_records, then
from support.database import parameters_by_function, require, scalar
from support.scenario import FactsToolContext


@then("the parameters for e2e::userDefinedTypes are")
def then_user_defined_parameters_are(context: FactsToolContext, table: Table) -> None:
    rows = table_records(table)
    symbol_ids = {
        row["symbol"]: scalar(
            context.facts_database_path,
            "SELECT id FROM symbol WHERE qualified_name=?",
            (row["symbol"],),
        )
        for row in rows
    }
    expected = [
        (int(row["position"]), row["name"], symbol_ids[row["symbol"]]) for row in rows
    ]
    actual = [
        (position, name, type_id)
        for position, name, type_id, _ in parameters_by_function(
            context.facts_database_path
        )["e2e::userDefinedTypes"]
    ]
    require(
        actual == expected,
        "user-defined struct, union, class, pointer, reference, array, enum, "
        f"and alias types did not resolve to their SymbolIds: {actual}",
    )


def primitive_parameters(context: FactsToolContext) -> list[tuple]:
    return parameters_by_function(context.facts_database_path)["e2e::primitiveTypes"]


@then("the primitive parameters for e2e::primitiveTypes are")
def then_primitive_parameters_are(context: FactsToolContext, table: Table) -> None:
    expected = [(int(row["position"]), row["name"]) for row in table_records(table)]
    actual = [
        (position, name) for position, name, _, _ in primitive_parameters(context)
    ]
    require(actual == expected, f"unexpected primitive type parameters: {actual}")


@then("their type IDs are positive predefined FileId-0 SymbolIds")
def then_primitive_type_ids_are_predefined(context: FactsToolContext) -> None:
    parameters = primitive_parameters(context)
    type_ids = [type_id for _, _, type_id, _ in parameters]
    require(
        all(type_id > 0 and type_id >> 32 == 0 for type_id in type_ids),
        "primitive and primitive-pointer types must use predefined FileId-0 "
        f"symbols: {parameters}",
    )


@then("their type IDs are all distinct")
def then_primitive_type_ids_are_distinct(context: FactsToolContext) -> None:
    parameters = primitive_parameters(context)
    type_ids = [type_id for _, _, type_id, _ in parameters]
    require(
        len(set(type_ids)) == len(type_ids),
        f"distinct primitive types must use distinct predefined SymbolIds: {parameters}",
    )
