import sqlite3

import pytest

from facts_tool.errors import FactsToolError
from facts_tool.schema import inspect_schema


@pytest.mark.parametrize("version", [10, 11])
def test_existing_queries_accept_additive_entry_schema(paired_databases, version):
    facts, _ = paired_databases
    with sqlite3.connect(facts) as database:
        database.execute(f"PRAGMA user_version={version}")
        identity = inspect_schema(database, "facts")
    assert identity.user_version == version


def test_unknown_entry_schema_is_rejected(paired_databases):
    facts, _ = paired_databases
    with sqlite3.connect(facts) as database:
        database.execute("PRAGMA user_version=12")
        with pytest.raises(FactsToolError, match="unsupported"):
            inspect_schema(database, "facts")
