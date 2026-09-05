import shutil
import sqlite3
from pathlib import Path

import pytest
from pytest_bdd import given, then

from facts_tool import FactsToolError, open_codebase


@given("project databases with invalid registry mappings")
def invalid_registries(paired_databases, tmp_path: Path, world):
    facts, project = paired_databases
    projects = []
    for name, sql in (
        ("missing", "DELETE FROM project_registry"),
        ("incomplete", "UPDATE project_registry SET complete=0"),
        ("mismatch", "UPDATE project_registry SET file_count=99"),
    ):
        candidate = tmp_path / f"{name}.sqlite"
        shutil.copy2(project, candidate)
        with sqlite3.connect(candidate) as db:
            db.execute(sql)
        projects.append(candidate)
    world["invalid_pairs"] = tuple(
        (facts, project, "E_DATABASE_PAIR") for project in projects
    )


@then("pair error codes are stable")
def pair_errors(world):
    codes = []
    for facts, project, _ in world["invalid_pairs"]:
        with pytest.raises(FactsToolError) as raised:
            open_codebase(facts_db=facts, project_db=project)
        codes.append(raised.value.code)
    assert codes == ["E_DATABASE_PAIR"] * 3
