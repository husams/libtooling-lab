from __future__ import annotations

import sqlite3
from pathlib import Path

from pytest_bdd import then, when
from steps.b027_external_callee_symbol_steps import ALIGNED_DELETE_USR
from steps.external_target_steps import extract_fixture
from support.database import query, require
from support.scenario import FactsToolContext


@when("B-027 runtime-target persistence is forced to fail on a rerun")
def when_runtime_target_persistence_fails(
    context: FactsToolContext, b027_sources: tuple[Path, ...]
) -> None:
    extract_fixture(context, b027_sources[0])
    with sqlite3.connect(context.facts_database_path) as connection:
        target = "(SELECT id FROM symbol WHERE usr=?)"
        for table in ("relation_site", "relation"):
            connection.execute(
                f"DELETE FROM {table} WHERE destination_id=" + target,
                (ALIGNED_DELETE_USR,),
            )
        connection.execute("DELETE FROM symbol WHERE usr=?", (ALIGNED_DELETE_USR,))
        connection.execute(
            "CREATE TRIGGER fail_b027 BEFORE INSERT ON symbol "
            f"WHEN NEW.usr='{ALIGNED_DELETE_USR}' BEGIN "
            "SELECT RAISE(ABORT, 'forced B-027 symbol failure'); END"
        )
    context.first_identities = query(context.facts_database_path, "SELECT * FROM symbol")
    extract_fixture(context, b027_sources[0])


@then("the relation-target failure is reported and the rerun is rolled back")
def then_runtime_target_failure_is_hard(context: FactsToolContext) -> None:
    require(context.last_returncode != 0, context.last_output)
    require("relation target resolution" in context.last_output, context.last_output)
    symbols = query(context.facts_database_path, "SELECT * FROM symbol")
    require(symbols == context.first_identities, "failed rerun changed symbols")
