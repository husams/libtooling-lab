import shutil
import sqlite3
from pathlib import Path

import pytest
from pytest_bdd import given, scenarios, then, when

from facts_tool import FactsToolError, open_codebase
from facts_tool.queryplan import codebase, nodes, out, start, symbol

from .database_capability import *  # noqa: F403
from .database_pairing import *  # noqa: F403

scenarios("../features/database.feature")


@when("successful invalid and error queries are attempted")
def read_only_queries(cb, world):
    results = []
    for _, database in cb:
        facts = Path(database.provenance.facts.path)
        project = Path(database.provenance.project.path)
        before = (facts.read_bytes(), project.read_bytes(), set(facts.parent.iterdir()))
        database.executor.run((start(codebase()) | nodes()).plan)
        with pytest.raises(FactsToolError):
            database.executor.run((start(symbol("missing")) | out("calls")).plan)
        after = (facts.read_bytes(), project.read_bytes(), set(facts.parent.iterdir()))
        results.append((before, after))
    world["preserved"] = results


@then("neither database nor adjacent files change")
def databases_preserved(world):
    assert all(before == after for before, after in world["preserved"])


@given("missing database paths")
def missing_paths(tmp_path: Path, world):
    world["pair"] = (
        tmp_path / "missing facts.sqlite",
        tmp_path / "missing project.sqlite",
    )


@given("one SQLite database path for both roles")
def same_path(paired_databases, world):
    world["pair"] = (paired_databases[0], paired_databases[0])


@when("I try to open the pair")
def open_invalid_pair(world):
    try:
        open_codebase(facts_db=world["pair"][0], project_db=world["pair"][1])
    except FactsToolError as exc:
        world["error"] = exc


@then("E_DATABASE is raised and no path is created")
def missing_result(world):
    assert world["error"].code == "E_DATABASE"
    assert all(not path.exists() for path in world["pair"])


@then("E_DATABASE_ROLE is raised")
def same_result(world):
    assert world["error"].code == "E_DATABASE_ROLE"


@given("reversed and incompatible databases")
def invalid_roles(paired_databases, tmp_path: Path, world):
    facts, project = paired_databases
    old = tmp_path / "facts-v9.sqlite"
    shutil.copy2(facts, old)
    with sqlite3.connect(old) as db:
        db.execute("DROP TABLE callable_return_type")
        db.execute("PRAGMA user_version=7")
    world["invalid_pairs"] = (
        (project, facts, "E_DATABASE_ROLE"),
        (old, project, "E_SCHEMA"),
    )


@when("I try each invalid pair")
def open_each_invalid(world):
    codes = []
    for facts, project, _ in world["invalid_pairs"]:
        with pytest.raises(FactsToolError) as raised:
            open_codebase(facts_db=facts, project_db=project)
        codes.append(raised.value.code)
    world["codes"] = codes


@then("role and schema error codes are stable")
def invalid_results(world):
    assert world["codes"] == [item[2] for item in world["invalid_pairs"]]
