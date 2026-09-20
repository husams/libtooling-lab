import sqlite3
from contextlib import closing

import pytest

from facts_tool.view_loader import ViewLoader


def test_closing_partial_iterator_releases_sqlite_read_lock(paired_databases):
    facts_path, project_path = paired_databases
    with (
        closing(sqlite3.connect(facts_path)) as facts,
        closing(sqlite3.connect(project_path)) as project,
        closing(sqlite3.connect(facts_path, timeout=0)) as writer,
    ):
        facts.row_factory = project.row_factory = sqlite3.Row
        rows = ViewLoader(facts, project).iter("edge")
        next(rows)
        writer.execute("UPDATE relation SET count=count+1")
        with pytest.raises(sqlite3.OperationalError, match="locked"):
            writer.commit()
        rows.close()
        writer.commit()
