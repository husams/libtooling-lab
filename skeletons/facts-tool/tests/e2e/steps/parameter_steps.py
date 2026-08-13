from __future__ import annotations

from support.bdd import step
from support.database import parameters_by_function, require, scalar
from support.scenario import FactsToolContext


@step("user-defined parameter forms resolve to their captured SymbolIds")
def then_user_defined_parameter_types_use_symbol_ids(
    context: FactsToolContext,
) -> None:
    parameters = parameters_by_function(context.facts_database_path)[
        "e2e::userDefinedTypes"
    ]
    expected_names = [
        "value",
        "pointer",
        "reference",
        "values",
        "mode",
        "count",
        "payload",
        "policy",
    ]
    require(
        [name for _, name, _, _ in parameters] == expected_names,
        f"unexpected user-defined type parameters: {parameters}",
    )

    symbol_ids = {
        name: scalar(
            context.facts_database_path,
            "SELECT id FROM symbol WHERE qualified_name=?",
            (name,),
        )
        for name in (
            "e2e::Widget",
            "e2e::Mode",
            "e2e::Count",
            "e2e::Payload",
            "e2e::Policy",
        )
    }
    require(
        [type_id for _, _, type_id, _ in parameters]
        == [
            symbol_ids["e2e::Widget"],
            symbol_ids["e2e::Widget"],
            symbol_ids["e2e::Widget"],
            symbol_ids["e2e::Widget"],
            symbol_ids["e2e::Mode"],
            symbol_ids["e2e::Count"],
            symbol_ids["e2e::Payload"],
            symbol_ids["e2e::Policy"],
        ],
        "user-defined struct, union, class, pointer, reference, array, enum, "
        f"and alias types did not resolve to their SymbolIds: {parameters}",
    )


@step("primitive parameter forms use distinct predefined FileId-0 SymbolIds")
def then_primitive_parameter_types_use_predefined_ids(
    context: FactsToolContext,
) -> None:
    parameters = parameters_by_function(context.facts_database_path)[
        "e2e::primitiveTypes"
    ]
    expected_names = ["signedValue", "enabled", "ratio", "text", "payload"]
    require(
        [name for _, name, _, _ in parameters] == expected_names,
        f"unexpected primitive type parameters: {parameters}",
    )
    type_ids = [type_id for _, _, type_id, _ in parameters]
    require(
        all(type_id > 0 and type_id >> 32 == 0 for type_id in type_ids),
        "primitive and primitive-pointer types must use predefined FileId-0 "
        f"symbols: {parameters}",
    )
    require(
        len(set(type_ids)) == len(type_ids),
        f"distinct primitive types must use distinct predefined SymbolIds: {parameters}",
    )
