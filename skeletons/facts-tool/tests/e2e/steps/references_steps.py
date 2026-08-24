from __future__ import annotations

from pytest_bdd import given, then, when
from support.database import query, require
from support.scenario import FactsToolContext
from support.table import Table, table_records


@given("a two-translation-unit reference project with shared inline and template bodies")
def given_reference_project(context: FactsToolContext) -> None:
    context.sources = tuple(
        (context.fixture_root / name).resolve(strict=True)
        for name in ("references_one.cpp", "references_two.cpp")
    )
    context.prepare()


@when("the real facts-tool indexes the reference project using compile_commands.json")
def when_reference_project_is_indexed(context: FactsToolContext) -> None:
    context.extract()


@then("the canonical Uses relations include")
def then_canonical_uses_relations_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (
            row["source"],
            row["destination"],
            int(row["count"]),
            int(row["sites"]),
        )
        for row in table_records(datatable)
    }
    actual = set(
        query(
            context.facts_database_path,
            "SELECT source.qualified_name,destination.qualified_name,r.count,"
            "COUNT(site.offset) FROM relation r "
            "JOIN symbol source ON source.id=r.source_id "
            "JOIN symbol destination ON destination.id=r.destination_id "
            "LEFT JOIN relation_site site ON site.source_id=r.source_id AND "
            "site.destination_id=r.destination_id AND site.kind=r.kind AND "
            "site.position=r.position WHERE r.kind=7 "
            "GROUP BY r.source_id,r.destination_id,r.kind,r.position",
        )
    )
    require(expected <= actual, f"missing canonical Uses relations: {expected - actual}")


@then("direct calls and construction targets are not stored as Uses")
def then_specific_relations_are_not_uses(context: FactsToolContext) -> None:
    forbidden = query(
        context.facts_database_path,
        "SELECT destination.qualified_name FROM relation r "
        "JOIN symbol destination ON destination.id=r.destination_id "
        "WHERE r.kind=7 AND (destination.qualified_name LIKE '%helper' OR "
        "destination.qualified_name LIKE '%Constructed::Constructed' OR "
        "destination.qualified_name LIKE '%sharedInline')",
    )
    require(not forbidden, f"specific relations were downgraded to Uses: {forbidden}")


@then("template and nested callable Uses have canonical owners")
def then_template_and_nested_owners_are_canonical(context: FactsToolContext) -> None:
    template_edges = query(
        context.facts_database_path,
        "SELECT source.usr,destination.qualified_name,r.count,COUNT(site.offset) "
        "FROM relation r JOIN symbol source ON source.id=r.source_id "
        "JOIN symbol destination ON destination.id=r.destination_id "
        "JOIN relation_site site ON site.source_id=r.source_id AND "
        "site.destination_id=r.destination_id AND site.kind=r.kind AND "
        "site.position=r.position WHERE r.kind=7 AND "
        "source.qualified_name='reference_fixture::templatedOwner' "
        "GROUP BY r.source_id,r.destination_id,r.kind,r.position",
    )
    require(
        len(template_edges) == 2
        and {row[1] for row in template_edges}
        == {
            "reference_fixture::primaryTarget",
            "reference_fixture::secondaryTarget",
        }
        and all(row[2:] == (1, 1) for row in template_edges),
        f"unexpected template ownership: {template_edges}",
    )

    nested_targets = {
        row[0]
        for row in query(
            context.facts_database_path,
            "SELECT destination.qualified_name FROM relation r "
            "JOIN symbol source ON source.id=r.source_id "
            "JOIN symbol destination ON destination.id=r.destination_id "
            "WHERE r.kind=7 AND (source.qualified_name LIKE '%Local%method%' "
            "OR source.qualified_name LIKE '%lambda%operator()%')",
        )
    }
    require(
        nested_targets
        == {
            "reference_fixture::primaryTarget",
            "reference_fixture::nestedDeclarations()::Local::localField",
            "reference_fixture::secondaryTarget",
        },
        f"unexpected nested callable ownership: {nested_targets}",
    )


@then("body-nested declarations retain their specialized facts")
def then_body_nested_declarations_retain_specialized_facts(
    context: FactsToolContext,
) -> None:
    enum_symbols = query(
        context.facts_database_path,
        "SELECT qualified_name FROM symbol WHERE qualified_name LIKE "
        "'%LocalEnum' OR qualified_name LIKE "
        "'%nestedDeclarations%LocalAlpha' OR qualified_name LIKE "
        "'%nestedDeclarations%LocalBeta'",
    )
    require(len(enum_symbols) == 3, f"missing nested enum facts: {enum_symbols}")

    field_facts = query(
        context.facts_database_path,
        "SELECT field.is_definition,owner.qualified_name FROM symbol field "
        "JOIN relation r ON r.source_id=field.id "
        "JOIN symbol owner ON owner.id=r.destination_id WHERE r.kind=8 AND "
        "field.qualified_name LIKE '%nestedDeclarations%localField' AND "
        "owner.qualified_name LIKE '%Local'",
    )
    require(
        field_facts and all(row[0] == 1 for row in field_facts),
        f"missing specialized nested field facts: {field_facts}",
    )


@then("every Uses site has an exact valid location with no duplicates")
def then_uses_sites_are_exact_and_distinct(context: FactsToolContext) -> None:
    invalid = query(
        context.facts_database_path,
        "SELECT source_id,destination_id,file_id,line,col,offset "
        "FROM relation_site WHERE kind=7 AND "
        "(file_id=0 OR line<1 OR col<1 OR offset<0)",
    )
    duplicates = query(
        context.facts_database_path,
        "SELECT source_id,destination_id,kind,position,file_id,offset,COUNT(*) "
        "FROM relation_site GROUP BY source_id,destination_id,kind,position,"
        "file_id,offset HAVING COUNT(*)<>1",
    )
    count_mismatches = query(
        context.facts_database_path,
        "SELECT r.source_id,r.destination_id,r.count,COUNT(site.offset) "
        "FROM relation r LEFT JOIN relation_site site ON "
        "site.source_id=r.source_id AND site.destination_id=r.destination_id "
        "AND site.kind=r.kind AND site.position=r.position WHERE r.kind=7 "
        "GROUP BY r.source_id,r.destination_id,r.kind,r.position "
        "HAVING r.count<>COUNT(site.offset)",
    )
    require(not invalid, f"invalid Uses sites: {invalid}")
    require(not duplicates, f"duplicate Uses sites: {duplicates}")
    require(not count_mismatches, f"stale Uses counts: {count_mismatches}")
