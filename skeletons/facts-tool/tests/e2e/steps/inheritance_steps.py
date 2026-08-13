from __future__ import annotations

from pytest_bdd import then
from support.database import query, require
from support.scenario import FactsToolContext
from support.table import Table, table_records


def direct_inheritance_relations(context: FactsToolContext) -> set[tuple]:
    return set(
        query(
            context.facts_database_path,
            "SELECT source.qualified_name,destination.qualified_name,"
            "r.kind,r.position,r.access,r.is_virtual_base,r.is_implicit,"
            "r.is_lexical,r.count FROM relation r "
            "JOIN symbol source ON source.id=r.source_id "
            "JOIN symbol destination ON destination.id=r.destination_id "
            "WHERE r.kind=2",
        )
    )


@then("the persisted direct inheritance fields include")
def then_direct_inheritance_relations_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (
            row["source"],
            row["destination"],
            int(row["kind"]),
            int(row["position"]),
            row["access"],
            int(row["is_virtual_base"]),
            int(row["is_implicit"]),
            int(row["is_lexical"]),
            int(row["count"]),
        )
        for row in table_records(datatable)
    }
    require(
        expected <= direct_inheritance_relations(context),
        f"missing direct inheritance relations: {expected}",
    )


@then("exactly 5 direct inheritance relations are stored")
def then_five_direct_inheritance_relations_are_stored(
    context: FactsToolContext,
) -> None:
    relations = direct_inheritance_relations(context)
    require(
        len(relations) == 5,
        f"expected 5 direct inheritance relations: {relations}",
    )
