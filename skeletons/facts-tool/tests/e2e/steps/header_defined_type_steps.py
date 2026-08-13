from __future__ import annotations

from pathlib import Path

from pytest_bdd import then
from support.database import file_snapshot, query, require, scalar
from support.scenario import FactsToolContext
from support.table import Table, table_records


@then("these symbols are stored in their declaring fixture files")
def then_symbols_are_stored_in_their_declaring_fixture_files(
    context: FactsToolContext, datatable: Table
) -> None:
    file_paths = dict(file_snapshot(context.files_database_path))
    expected = {
        row["qualified_name"]: str(
            (context.fixture_root / row["fixture"]).resolve(strict=True)
        )
        for row in table_records(datatable)
    }
    placeholders = ",".join("?" for _ in expected)
    actual = {
        qualified_name: file_paths[file_id]
        for qualified_name, file_id in query(
            context.facts_database_path,
            "SELECT qualified_name,file_id FROM symbol "
            f"WHERE qualified_name IN ({placeholders})",
            tuple(expected),
        )
    }
    require(actual == expected, f"unexpected symbol fixture files: {actual}")
    require(
        all(Path(path).is_absolute() for path in actual.values()),
        f"symbol paths must be canonical: {actual}",
    )


def expected_type_id(context: FactsToolContext, resolved_type: str) -> int:
    if resolved_type == "predefined:int":
        return scalar(
            context.facts_database_path,
            "SELECT p.type FROM parameter p "
            "JOIN symbol s ON s.id=p.symbol_id "
            "WHERE s.qualified_name='e2e::primitiveTypes' "
            "AND p.name='signedValue'",
        )
    return scalar(
        context.facts_database_path,
        "SELECT id FROM symbol WHERE qualified_name=?",
        (resolved_type,),
    )


@then("the persisted parameters for e2e::fun are")
def then_persisted_parameters_for_fun_are(
    context: FactsToolContext, datatable: Table
) -> None:
    fields = (
        "is_pointer",
        "is_lvalue_reference",
        "is_rvalue_reference",
        "is_forwarding_reference",
        "is_const",
        "is_pack",
        "has_default",
    )
    expected = [
        (
            int(row["position"]),
            row["name"],
            expected_type_id(context, row["resolved_type"]),
            *(int(row[field]) for field in fields),
        )
        for row in table_records(datatable)
    ]
    actual = query(
        context.facts_database_path,
        "SELECT p.position,p.name,p.type,p.is_pointer,p.is_lvalue_reference,"
        "p.is_rvalue_reference,p.is_forwarding_reference,p.is_const,p.is_pack,"
        "p.has_default FROM parameter p JOIN symbol s ON s.id=p.symbol_id "
        "WHERE s.qualified_name='e2e::fun' ORDER BY p.position",
    )
    require(actual == expected, f"unexpected persisted fun parameters: {actual}")
