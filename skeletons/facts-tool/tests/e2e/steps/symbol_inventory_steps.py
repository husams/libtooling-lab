from __future__ import annotations

from collections.abc import Iterable

from support.bdd import step
from support.database import parameters_by_function, query, require
from support.scenario import FactsToolContext


def require_node(
    context: FactsToolContext, node: int, expected_names: Iterable[str]
) -> None:
    actual = {
        name
        for (name,) in query(
            context.facts_database_path,
            "SELECT qualified_name FROM symbol WHERE node=?",
            (node,),
        )
    }
    expected = set(expected_names)
    require(
        expected <= actual,
        f"node {node} missing {expected - actual}; actual={actual}",
    )


@step("concrete supported symbol types are present")
def then_concrete_symbols_are_present(context: FactsToolContext) -> None:
    require_node(
        context,
        1,
        {
            "e2e::headerHelper",
            "e2e::primitiveTypes",
            "e2e::transform",
            "e2e::useOne",
            "e2e::useTwo",
            "e2e::userDefinedTypes",
        },
    )
    require_node(
        context,
        2,
        {
            "e2e::CompositeWidget",
            "e2e::Deferred",
            "e2e::Payload",
            "e2e::Policy",
            "e2e::PrivateWidget",
            "e2e::PublicWidget",
            "e2e::Widget",
        },
    )
    require_node(context, 3, {"e2e::Mode"})
    require_node(
        context,
        4,
        {
            "e2e::Widget::value",
            "e2e::Mode::Fast",
            "e2e::Mode::Slow",
            "e2e::sharedCounter",
        },
    )
    require_node(context, 5, {"e2e"})
    require_node(context, 6, {"e2e::Count"})


@step("function definitions, parameter names, and defaults are present")
def then_function_definitions_and_parameters_are_present(
    context: FactsToolContext,
) -> None:
    defined = {
        name
        for (name,) in query(
            context.facts_database_path,
            "SELECT s.qualified_name FROM definition d "
            "JOIN symbol s ON s.id=d.symbol_id",
        )
    }
    expected_definitions = {
        "e2e::headerHelper",
        "e2e::transform",
        "e2e::useOne",
        "e2e::useTwo",
    }
    require(
        expected_definitions <= defined,
        f"missing function definitions: {expected_definitions - defined}",
    )

    parameters = parameters_by_function(context.facts_database_path)
    require(
        [name for _, name, _, _ in parameters["e2e::transform"]]
        == ["widget", "factor"],
        f"unexpected transform parameters: {parameters['e2e::transform']}",
    )
    helper_parameters = parameters["e2e::headerHelper"]
    require(
        [name for _, name, _, _ in helper_parameters] == ["input", "delta"]
        and helper_parameters[1][3] == 1,
        f"default parameter was not captured: {helper_parameters}",
    )


@step("typed facts and relations reference captured symbols")
def then_typed_facts_and_relations_are_valid(context: FactsToolContext) -> None:
    require(
        not query(context.facts_database_path, "PRAGMA foreign_key_check"),
        "facts database has broken typed or relation references",
    )
    require(
        not query(
            context.facts_database_path,
            "SELECT r.source_id,r.destination_id FROM relation r "
            "LEFT JOIN symbol s ON s.id=r.source_id "
            "LEFT JOIN symbol d ON d.id=r.destination_id "
            "WHERE s.id IS NULL OR d.id IS NULL",
        ),
        "relation rows must reference captured symbols",
    )
