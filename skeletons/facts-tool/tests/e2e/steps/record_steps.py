from __future__ import annotations

from support.bdd import step
from support.database import query, require
from support.scenario import FactsToolContext

_RECORDS = {
    "e2e::CompositeWidget",
    "e2e::Deferred",
    "e2e::Payload",
    "e2e::Policy",
    "e2e::PrivateWidget",
    "e2e::PublicWidget",
    "e2e::Widget",
}


@step("struct, union, and class declarations and definitions use record facts")
def then_cpp_record_kinds_use_record_facts(context: FactsToolContext) -> None:
    records = {
        qualified_name: node
        for qualified_name, node in query(
            context.facts_database_path,
            "SELECT qualified_name,node FROM symbol "
            "WHERE qualified_name IN (?,?,?,?,?,?,?)",
            tuple(sorted(_RECORDS)),
        )
    }
    require(
        records.keys() == _RECORDS,
        f"missing C++ record kinds: {_RECORDS - records.keys()}",
    )
    require(
        all(node == 2 for node in records.values()),
        f"struct, union, and class must use Record storage: {records}",
    )


@step("defined records have definitions and forward-only records do not")
def then_record_definition_state_is_stored(context: FactsToolContext) -> None:
    defined = {
        name
        for (name,) in query(
            context.facts_database_path,
            "SELECT s.qualified_name FROM definition d "
            "JOIN symbol s ON s.id=d.symbol_id",
        )
    }
    expected_definitions = _RECORDS - {"e2e::Deferred"}
    require(
        expected_definitions <= defined,
        f"missing record definitions: {expected_definitions - defined}",
    )
    require(
        "e2e::Deferred" not in defined,
        "forward-only record must not have a definition",
    )
