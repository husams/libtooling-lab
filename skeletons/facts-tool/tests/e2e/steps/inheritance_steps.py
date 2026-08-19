from __future__ import annotations

from pytest_bdd import then, when
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


@then("exactly 6 direct inheritance relations are stored")
def then_six_direct_inheritance_relations_are_stored(
    context: FactsToolContext,
) -> None:
    relations = direct_inheritance_relations(context)
    require(
        len(relations) == 6,
        f"expected 6 direct inheritance relations: {relations}",
    )


@then("the external inheritance target is queryable without its header body")
def then_external_inheritance_target_is_lightweight(
    context: FactsToolContext,
) -> None:
    relation = query(
        context.facts_database_path,
        "SELECT destination.qualified_name,destination.is_external,"
        "destination.is_definition FROM relation r "
        "JOIN symbol source ON source.id=r.source_id "
        "JOIN symbol destination ON destination.id=r.destination_id "
        "WHERE r.kind=2 AND source.qualified_name='e2e::ExternalWidget'",
    )
    external_symbols = query(
        context.facts_database_path,
        "SELECT qualified_name FROM symbol "
        "WHERE qualified_name LIKE 'external::%' ORDER BY qualified_name",
    )
    require(
        relation == [("external::Base", 1, 0)],
        f"unexpected external inheritance target: {relation}",
    )
    require(
        external_symbols == [("external::Base",)],
        f"system header body was indexed: {external_symbols}",
    )


@when("relation persistence is forced to fail on a rerun")
def when_relation_persistence_fails(context: FactsToolContext) -> None:
    context.force_relation_persistence_failure()


@then("the indexing command exits unsuccessfully")
def then_indexing_exits_unsuccessfully(context: FactsToolContext) -> None:
    require(
        context.last_returncode is not None and context.last_returncode != 0,
        f"expected a nonzero exit code: {context.last_returncode}",
    )


@then("the relation failure diagnostic identifies both symbols and the base USR")
def then_relation_failure_is_specific(context: FactsToolContext) -> None:
    required = (
        "indexing incomplete",
        "relation=inheritance",
        "derived='",
        "base='",
        "usr='",
    )
    require(
        all(fragment in context.last_output for fragment in required),
        f"incomplete relation diagnostic:\n{context.last_output}",
    )
    require(
        "No such file or directory" not in context.last_output,
        f"misleading filesystem diagnostic:\n{context.last_output}",
    )
