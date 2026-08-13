from __future__ import annotations

from support.bdd import Table, table_records, then
from support.database import query, require
from support.scenario import FactsToolContext


def direct_inheritance_relations(context: FactsToolContext) -> set[tuple]:
    return set(
        query(
            context.facts_database_path,
            "SELECT source.qualified_name,destination.qualified_name,"
            "r.kind,r.position,r.flags,r.count FROM relation r "
            "JOIN symbol source ON source.id=r.source_id "
            "JOIN symbol destination ON destination.id=r.destination_id "
            "WHERE r.kind=2",
        )
    )


@then("the direct inheritance relations include")
def then_direct_inheritance_relations_include(
    context: FactsToolContext, table: Table
) -> None:
    expected = {
        (
            row["source"],
            row["destination"],
            int(row["kind"]),
            int(row["position"]),
            int(row["flags"]),
            int(row["count"]),
        )
        for row in table_records(table)
    }
    require(
        expected <= direct_inheritance_relations(context),
        f"missing direct inheritance relations: {expected}",
    )


@then("exactly 4 direct inheritance relations are stored")
def then_four_direct_inheritance_relations_are_stored(
    context: FactsToolContext,
) -> None:
    relations = direct_inheritance_relations(context)
    require(
        len(relations) == 4,
        f"expected 4 direct inheritance relations: {relations}",
    )
