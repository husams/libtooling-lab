from __future__ import annotations

from support.bdd import step
from support.database import query, require
from support.scenario import FactsToolContext


@step("direct inheritance uses SymbolId relations with access and virtual flags")
def then_direct_inheritance_relations_are_stored(context: FactsToolContext) -> None:
    relations = set(
        query(
            context.facts_database_path,
            "SELECT source.qualified_name,destination.qualified_name,"
            "r.kind,r.position,r.flags,r.count FROM relation r "
            "JOIN symbol source ON source.id=r.source_id "
            "JOIN symbol destination ON destination.id=r.destination_id "
            "WHERE r.kind=2",
        )
    )
    expected = {
        ("e2e::CompositeWidget", "e2e::Policy", 2, 1, 5, 1),
        ("e2e::CompositeWidget", "e2e::Widget", 2, 0, 0, 1),
        ("e2e::PrivateWidget", "e2e::Widget", 2, 0, 2, 1),
        ("e2e::PublicWidget", "e2e::Widget", 2, 0, 0, 1),
    }
    require(
        relations == expected,
        f"unexpected direct inheritance relations: {relations}",
    )
