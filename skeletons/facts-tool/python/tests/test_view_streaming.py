import sqlite3
from contextlib import closing

import pytest

from facts_tool import FactsToolError
from facts_tool.view_loader import ViewLoader


@pytest.mark.parametrize(
    "view",
    [
        "symbol",
        "parameter",
        "template_parameter",
        "template_argument",
        "edge",
        "site",
        "definition",
        "enumeration",
        "enumerator",
        "initializer",
        "return_type",
        "repository",
        "clone",
        "component",
        "directory",
        "file",
    ],
)
def test_view_iterator_defers_sql_until_consumed(paired_databases, view):
    facts_path, project_path = paired_databases
    with (
        closing(sqlite3.connect(facts_path)) as facts,
        closing(sqlite3.connect(project_path)) as project,
    ):
        facts.row_factory = project.row_factory = sqlite3.Row
        statements = []
        facts.set_trace_callback(statements.append)
        project.set_trace_callback(statements.append)
        rows = ViewLoader(facts, project).iter(view)
        assert not statements
        next(rows, None)
        assert statements
        rows.close()


def test_definition_iterator_decodes_only_requested_rows(paired_databases):
    facts_path, project_path = paired_databases
    with (
        closing(sqlite3.connect(facts_path)) as facts,
        closing(sqlite3.connect(project_path)) as project,
    ):
        facts.row_factory = project.row_factory = sqlite3.Row
        facts.execute("DELETE FROM definition")
        facts.executemany(
            "INSERT INTO definition VALUES(?,?,0,1)",
            [((1 << 32) | 1, 1), ((1 << 32) | 2, 999)],
        )
        rows = ViewLoader(facts, project).iter("definition")
        assert next(rows)["file_id"] == 1
        with pytest.raises(FactsToolError, match="E_IDENTITY"):
            next(rows)


def test_evidence_joins_capture_provenance_without_per_row_sql(schema13_pair):
    facts_path, project_path, _ = schema13_pair
    with (
        closing(sqlite3.connect(facts_path)) as facts,
        closing(sqlite3.connect(project_path)) as project,
    ):
        facts.row_factory = project.row_factory = sqlite3.Row
        statements = []
        facts.set_trace_callback(statements.append)
        rows = ViewLoader(facts, project).load("expression_occurrence")
        assert len(rows) == 2
        assert all(not row["_capture_provenance_missing"] for row in rows)
        selects = [sql for sql in statements if sql.startswith("SELECT")]
        assert len(selects) == 1
        assert "LEFT JOIN facts_project_provenance" in selects[0]
