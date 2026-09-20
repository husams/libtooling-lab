import sqlite3
from pathlib import Path
from unittest.mock import patch

import pytest
from support.symbol_factory import COLUMNS, symbol

from facts_tool import open_codebase
from facts_tool.paths import FileResolver
from facts_tool.view_symbols import _symbol, load_symbols, lookup_symbol


@pytest.mark.parametrize("ref", ["c:@F@run#", "app::run", "run"])
def test_lookup_hydrates_only_matching_symbols(paired_databases, ref):
    facts, project = paired_databases
    with open_codebase(facts_db=facts, project_db=project) as cb:
        with patch("facts_tool.view_symbols._symbol", wraps=_symbol) as decode:
            rows = lookup_symbol(cb._facts, cb.executor.loader.files, ref)
        assert [row["qualified_name"] for row in rows] == ["app::run"]
        assert decode.call_count == 1


def test_lookup_preserves_usr_precedence(paired_databases):
    facts, project = paired_databases
    with sqlite3.connect(facts) as db:
        db.execute("UPDATE symbol SET qualified_name='c:@F@run#' WHERE id=?", (1,))
    with open_codebase(facts_db=facts, project_db=project) as cb:
        rows = lookup_symbol(cb._facts, cb.executor.loader.files, "c:@F@run#")
        assert [row["qualified_name"] for row in rows] == ["app::run"]


def test_id_hydration_batches_and_preserves_signed_order(paired_databases):
    facts, project = paired_databases
    ids = {*range(1000, 1601), (1 << 63) + 1}
    with sqlite3.connect(facts) as db:
        placeholders = ",".join("?" for _ in COLUMNS)
        db.executemany(
            f"INSERT INTO symbol({','.join(COLUMNS)}) VALUES({placeholders})",
            [
                symbol(
                    i if i < 1 << 63 else i - (1 << 64), 1, 13, f"usr{i}", f"name{i}"
                )
                for i in ids
            ],
        )
    with sqlite3.connect(facts) as db:
        db.row_factory = sqlite3.Row
        with patch.object(FileResolver, "path", return_value=None):
            rows = load_symbols(db, FileResolver(db), ids)
        assert [row["id"] for row in rows] == [(1 << 63) + 1, *range(1000, 1601)]
        assert load_symbols(db, FileResolver(db), set()) == []


@pytest.mark.parametrize(
    ("ref", "index"),
    [("c:@F@run#", "idx_symbol_unique_usr"), ("app::run", "idx_symbol_qualified_name")],
)
def test_lookup_uses_native_schema_indexes(paired_databases, ref, index):
    schema_file = Path(__file__).parents[2] / "src/storage/Schema.h"
    schema = schema_file.read_text().split('R"sql(', 1)[1].split(')sql"', 1)[0]
    with sqlite3.connect(":memory:") as db:
        db.row_factory = sqlite3.Row
        db.executescript(schema)
        placeholders = ",".join("?" for _ in COLUMNS)
        db.execute(
            f"INSERT INTO symbol({','.join(COLUMNS)}) VALUES({placeholders})",
            symbol(1, 1, 13, "c:@F@run#", "app::run"),
        )
        statements = []
        db.set_trace_callback(statements.append)
        lookup_symbol(db, FileResolver(db), ref)
        db.set_trace_callback(None)
        plans = [
            str(row[3])
            for statement in statements
            for row in db.execute("EXPLAIN QUERY PLAN " + statement)
        ]
        assert any("SEARCH s USING INDEX " + index in step for step in plans)
