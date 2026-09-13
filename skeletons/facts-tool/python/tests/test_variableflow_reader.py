import sqlite3

import pytest
from support.variableflow_data import create_empty_flow_db

from facts_tool import FactsToolError, open_variable_flow


def test_reader_close_and_decode_errors_are_public_failures(tmp_path) -> None:
    path = tmp_path / "flow.db"
    create_empty_flow_db(path)
    reader = open_variable_flow(path)
    reader.close()
    with pytest.raises(FactsToolError, match="E_SOURCE"):
        reader.runs()
    with pytest.raises(FactsToolError, match="E_SOURCE"):
        reader.get(1)

    reader = open_variable_flow(path)
    real_db = reader._db

    class BrokenDB:
        def execute(self, sql, *args):
            if "variable_flow_node" in sql:
                raise sqlite3.OperationalError("broken child read")
            return real_db.execute(sql, *args)

    reader._db = BrokenDB()  # type: ignore[assignment]
    with pytest.raises(FactsToolError, match="E_SOURCE"):
        reader.get(1)
