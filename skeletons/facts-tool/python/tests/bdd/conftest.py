from pathlib import Path

import pytest
from pytest_bdd import given

from facts_tool import CodeBase, open_codebase


@pytest.fixture
def world() -> dict[str, object]:
    return {}


@pytest.fixture
def codebase(paired_databases: tuple[Path, Path]) -> CodeBase:
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as value:
        yield value


@given("a valid paired facts and project database", target_fixture="cb")
def valid_pair(codebase: CodeBase) -> CodeBase:
    return codebase
