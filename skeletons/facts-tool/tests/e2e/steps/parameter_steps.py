from __future__ import annotations

from pytest_bdd import then
from support.database import parameters_by_function, query, require, scalar
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("the persisted parameters for e2e::userDefinedTypes include")
def then_user_defined_parameters_include(
    context: FactsToolContext, datatable: Table
) -> None:
    rows = table_records(datatable)
    symbol_ids = {
        row["type_qualified_name"]: scalar(
            context.facts_database_path,
            "SELECT id FROM symbol WHERE qualified_name=?",
            (row["type_qualified_name"],),
        )
        for row in rows
    }
    semantic_fields = (
        "is_pointer",
        "is_lvalue_reference",
        "is_rvalue_reference",
        "is_forwarding_reference",
        "is_const",
        "is_pack",
    )
    expected = [
        (
            int(row["position"]),
            row["name"],
            symbol_ids[row["type_qualified_name"]],
            *(int(row[field]) for field in semantic_fields),
        )
        for row in rows
    ]
    actual = query(
        context.facts_database_path,
        "SELECT p.position,p.name,p.type,p.is_pointer,p.is_lvalue_reference,"
        "p.is_rvalue_reference,p.is_forwarding_reference,p.is_const,p.is_pack "
        "FROM parameter p JOIN symbol s ON s.id=p.symbol_id "
        "WHERE s.qualified_name='e2e::userDefinedTypes' ORDER BY p.position",
    )
    require(
        all(parameter in actual for parameter in expected),
        "user-defined struct, union, class, pointer, reference, array, enum, "
        f"and alias types did not resolve to their SymbolIds: {actual}",
    )


@then("e2e::userDefinedTypes has exactly 8 parameters")
def then_user_defined_types_has_eight_parameters(
    context: FactsToolContext,
) -> None:
    parameters = parameters_by_function(context.facts_database_path)[
        "e2e::userDefinedTypes"
    ]
    require(
        len(parameters) == 8,
        f"expected 8 user-defined type parameters: {parameters}",
    )


def primitive_parameters(context: FactsToolContext) -> list[tuple]:
    return parameters_by_function(context.facts_database_path)["e2e::primitiveTypes"]


@then("the persisted primitive parameter fields for e2e::primitiveTypes are")
def then_primitive_parameters_are(context: FactsToolContext, datatable: Table) -> None:
    semantic_fields = (
        "is_pointer",
        "is_lvalue_reference",
        "is_rvalue_reference",
        "is_forwarding_reference",
        "is_const",
        "is_pack",
    )
    expected = [
        (
            int(row["position"]),
            row["name"],
            *(int(row[field]) for field in semantic_fields),
        )
        for row in table_records(datatable)
    ]
    actual = query(
        context.facts_database_path,
        "SELECT p.position,p.name,p.is_pointer,p.is_lvalue_reference,"
        "p.is_rvalue_reference,p.is_forwarding_reference,p.is_const,p.is_pack "
        "FROM parameter p JOIN symbol s ON s.id=p.symbol_id "
        "WHERE s.qualified_name='e2e::primitiveTypes' ORDER BY p.position",
    )
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
