from __future__ import annotations

from support.bdd import Table, table_records, then
from support.database import query, require
from support.scenario import FactsToolContext


@then("the direct inheritance relations are")
def then_direct_inheritance_relations_are(
    context: FactsToolContext, table: Table
) -> None:
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
        relations == expected,
        f"unexpected direct inheritance relations: {relations}",
    )
