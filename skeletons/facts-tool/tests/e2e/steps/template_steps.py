from __future__ import annotations

from pytest_bdd import then
from support.database import query, require
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("the template symbols include")
def then_template_symbols_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (row["qualified_name"], int(row["node"]), int(row["kind"]))
        for row in table_records(datatable)
    }
    actual = set(
        query(
            context.facts_database_path,
            "SELECT qualified_name,node,kind FROM symbol WHERE id IN "
            "(SELECT DISTINCT symbol_id FROM template_argument)",
        )
    )
    require(expected <= actual, f"missing template symbols: {expected - actual}")


@then("the declared template arguments are")
def then_declared_template_arguments_are(
    context: FactsToolContext, datatable: Table
) -> None:
    fields = (
        "position",
        "is_parameter_pack",
        "is_non_type",
        "is_template_template",
    )
    expected = {
        (
            row["qualified_name"],
            int(row[fields[0]]),
            row["name"],
            *(int(row[field]) for field in fields[1:]),
        )
        for row in table_records(datatable)
    }
    names = tuple({row[0] for row in expected})
    placeholders = ",".join("?" for _ in names)
    actual = set(
        query(
            context.facts_database_path,
            "SELECT s.qualified_name,a.position,a.name,a.is_parameter_pack,"
            "a.is_non_type,a.is_template_template FROM template_argument a "
            "JOIN symbol s ON s.id=a.symbol_id "
            f"WHERE s.qualified_name IN ({placeholders})",
            names,
        )
    )
    require(
        actual == expected,
        f"unexpected declared template arguments: {actual}",
    )


@then("every non-type template argument has a predefined type ID")
def then_non_type_arguments_have_predefined_type_ids(
    context: FactsToolContext,
) -> None:
    type_ids = [
        type_id
        for (type_id,) in query(
            context.facts_database_path,
            "SELECT type_id FROM template_argument WHERE is_non_type=1",
        )
    ]
    require(type_ids, "expected at least one non-type template argument")
    require(
        all(type_id > 0 and type_id >> 32 == 0 for type_id in type_ids),
        f"non-type template arguments must use predefined type IDs: {type_ids}",
    )


@then("the supplied template parameters include")
def then_supplied_template_parameters_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (
            row["qualified_name"],
            int(row["position"]),
            row["value"],
            row["type"],
            int(row["kind"]),
            int(row["pack_index"]),
        )
        for row in table_records(datatable)
    }
    actual = set(
        query(
            context.facts_database_path,
            "SELECT s.qualified_name,p.position,p.value,"
            "COALESCE(t.qualified_name,''),p.kind,p.pack_index "
            "FROM template_parameter p "
            "JOIN symbol s ON s.id=p.symbol_id "
            "LEFT JOIN symbol t ON t.id=p.type_id",
        )
    )
    require(
        expected <= actual,
        f"missing supplied template parameters: {expected - actual}",
    )


@then("a partial record specialization retains open slots and supplied values")
def then_partial_record_specialization_retains_both_sides(
    context: FactsToolContext,
) -> None:
    [(count,)] = query(
        context.facts_database_path,
        "SELECT COUNT(*) FROM symbol s "
        "WHERE s.qualified_name='e2e::StructTemplate' "
        "AND EXISTS (SELECT 1 FROM template_argument declared "
        "            WHERE declared.symbol_id=s.id) "
        "AND EXISTS (SELECT 1 FROM template_parameter supplied "
        "            WHERE supplied.symbol_id=s.id)",
    )
    require(count > 0, "expected a persisted partial record specialization")


@then("the template instance relations include")
def then_template_instance_relations_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (row["source"], row["destination"], int(row["kind"]))
        for row in table_records(datatable)
    }
    actual = set(
        query(
            context.facts_database_path,
            "SELECT DISTINCT source.qualified_name,destination.qualified_name,"
            "relation.kind FROM relation "
            "JOIN symbol source ON source.id=relation.source_id "
            "JOIN symbol destination ON destination.id=relation.destination_id "
            "WHERE relation.kind IN (4,5) "
            "AND EXISTS (SELECT 1 FROM template_parameter supplied "
            "            WHERE supplied.symbol_id=source.id) "
            "AND EXISTS (SELECT 1 FROM template_argument declared "
            "            WHERE declared.symbol_id=destination.id)",
        )
    )
    require(expected <= actual, f"missing template relations: {expected - actual}")


@then("the template argument type relations include")
def then_template_argument_type_relations_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (row["source"], row["destination"], int(row["position"]))
        for row in table_records(datatable)
    }
    actual = set(
        query(
            context.facts_database_path,
            "SELECT DISTINCT source.qualified_name,destination.qualified_name,"
            "relation.position FROM relation "
            "JOIN symbol source ON source.id=relation.source_id "
            "JOIN symbol destination ON destination.id=relation.destination_id "
            "WHERE relation.kind=23 "
            "AND EXISTS (SELECT 1 FROM template_parameter supplied "
            "            WHERE supplied.symbol_id=source.id)",
        )
    )
    require(
        expected <= actual,
        f"missing template argument type relations: {expected - actual}",
    )


@then("the persisted instance method ownership relations include")
def then_instance_method_ownership_relations_include(
    context: FactsToolContext, datatable: Table
) -> None:
    expected = {
        (row["source"], row["destination"], int(row["kind"]))
        for row in table_records(datatable)
    }
    actual = set(
        query(
            context.facts_database_path,
            "SELECT DISTINCT source.qualified_name,destination.qualified_name,"
            "relation.kind FROM relation "
            "JOIN symbol source ON source.id=relation.source_id "
            "JOIN symbol destination ON destination.id=relation.destination_id "
            "WHERE relation.kind=9 "
            "AND EXISTS (SELECT 1 FROM template_parameter supplied "
            "            WHERE supplied.symbol_id=source.id)",
        )
    )
    require(
        expected <= actual,
        f"missing instance method ownership relations: {expected - actual}",
    )


@then("a partial record specialization points to its primary template")
def then_partial_record_specialization_points_to_primary(
    context: FactsToolContext,
) -> None:
    [(count,)] = query(
        context.facts_database_path,
        "SELECT COUNT(*) FROM relation specialization "
        "JOIN symbol partial ON partial.id=specialization.source_id "
        "JOIN symbol primary_template "
        "ON primary_template.id=specialization.destination_id "
        "WHERE specialization.kind=4 "
        "AND partial.qualified_name='e2e::StructTemplate' "
        "AND primary_template.qualified_name='e2e::StructTemplate' "
        "AND EXISTS (SELECT 1 FROM template_argument open_slot "
        "            WHERE open_slot.symbol_id=partial.id) "
        "AND EXISTS (SELECT 1 FROM template_parameter pattern_argument "
        "            WHERE pattern_argument.symbol_id=partial.id) "
        "AND EXISTS (SELECT 1 FROM template_argument primary_slot "
        "            WHERE primary_slot.symbol_id=primary_template.id) "
        "AND NOT EXISTS (SELECT 1 FROM template_parameter primary_argument "
        "                WHERE primary_argument.symbol_id=primary_template.id)",
    )
    require(
        count == 1,
        f"expected one partial-to-primary specialization relation, found {count}",
    )


@then(
    "a concrete record instance selected through the partial specialization "
    "points to it"
)
def then_concrete_record_instance_points_to_partial(
    context: FactsToolContext,
) -> None:
    [(count,)] = query(
        context.facts_database_path,
        "SELECT COUNT(*) FROM relation instantiation "
        "JOIN symbol concrete ON concrete.id=instantiation.source_id "
        "JOIN symbol partial ON partial.id=instantiation.destination_id "
        "WHERE instantiation.kind=5 "
        "AND concrete.qualified_name='e2e::StructTemplate' "
        "AND partial.qualified_name='e2e::StructTemplate' "
        "AND EXISTS (SELECT 1 FROM template_parameter concrete_type "
        "            JOIN symbol concrete_type_symbol "
        "              ON concrete_type_symbol.id=concrete_type.type_id "
        "            WHERE concrete_type.symbol_id=concrete.id "
        "              AND concrete_type.position=0 "
        "              AND concrete_type.is_pointer=1 "
        "              AND concrete_type_symbol.qualified_name='e2e::Widget') "
        "AND EXISTS (SELECT 1 FROM template_parameter concrete_value "
        "            WHERE concrete_value.symbol_id=concrete.id "
        "              AND concrete_value.position=1 "
        "              AND concrete_value.value='7') "
        "AND EXISTS (SELECT 1 FROM template_argument open_slot "
        "            WHERE open_slot.symbol_id=partial.id) "
        "AND EXISTS (SELECT 1 FROM template_parameter pattern_argument "
        "            WHERE pattern_argument.symbol_id=partial.id "
        "              AND pattern_argument.position=0 "
        "              AND pattern_argument.is_pointer=1) "
        "AND EXISTS (SELECT 1 FROM relation specialization "
        "            WHERE specialization.source_id=partial.id "
        "              AND specialization.kind=4)",
    )
    require(
        count == 1,
        f"expected one concrete-to-partial instantiation relation, found {count}",
    )
