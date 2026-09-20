import sqlite3
from unittest.mock import patch

import pytest

from facts_tool import open_codebase
from facts_tool.ids import MASK64, SymbolId
from facts_tool.queryplan import eq
from facts_tool.state import ExecutionState
from facts_tool.symbol_stream import stream_symbol_nodes
from facts_tool.symbol_window import symbol_window
from facts_tool.view_symbols import _symbol


@pytest.mark.parametrize("after", [None, -10, 0, 1, 1 << 63, MASK64 - 1, MASK64])
@pytest.mark.parametrize("limit", [0, 1, 2, 3, 10])
def test_symbol_window_preserves_pre_filter_signed_order_and_unsigned_cursor(
    after, limit
):
    ids = [-(1 << 63), -1, 0, 1, 2]
    with sqlite3.connect(":memory:") as db:
        db.execute("CREATE TABLE symbol(id INTEGER PRIMARY KEY)")
        db.executemany("INSERT INTO symbol VALUES(?)", [(value,) for value in ids])
        window = symbol_window(db, after, limit)
        assert window is not None
        where, params = window.predicate
        actual = [
            row[0]
            for row in db.execute(
                f"SELECT s.id FROM symbol s WHERE {where} ORDER BY s.id", params
            )
        ]
    expected = [
        value for value in ids if after is None or SymbolId.unpack(value).packed > after
    ]
    assert actual == expected[:limit]
    assert window.truncated == (len(expected) > limit)
    assert window.cursor == (
        str(SymbolId.unpack(expected[:limit][-1]).packed) if expected[:limit] else None
    )


def test_symbol_window_uses_fallback_for_lexical_cursors():
    with sqlite3.connect(":memory:") as db:
        assert symbol_window(db, "symbol:10", 5) is None
        assert symbol_window(db, None, -1) is None


def test_stream_pushes_indexed_predicate_after_enumeration_budget(paired_databases):
    facts, project = paired_databases
    with sqlite3.connect(facts) as db:
        db.execute("CREATE INDEX idx_symbol_unique_usr ON symbol(usr)")
    with open_codebase(facts_db=facts, project_db=project) as cb:
        state = ExecutionState()
        statements = []
        cb._facts.set_trace_callback(statements.append)
        with patch("facts_tool.view_symbols._symbol", wraps=_symbol) as decode:
            stream = stream_symbol_nodes(
                cb.executor.loader, eq("usr", "c:@F@save#"), None, 2, state
            )
            assert stream is not None
            assert list(stream) == []
            assert state.truncated and state.cursor == str((1 << 32) + 1)
            stream = stream_symbol_nodes(
                cb.executor.loader, eq("usr", "c:@F@save#"), None, 3, state
            )
            assert stream is not None
            assert [row["qualified_name"] for row in stream] == ["app::save"]
        assert decode.call_count == 1
        cb._facts.set_trace_callback(None)
        plans = [
            str(row[3])
            for sql in statements
            if "s.*" in sql
            for row in cb._facts.execute("EXPLAIN QUERY PLAN " + sql)
        ]
        assert any(
            "SEARCH s USING INDEX idx_symbol_unique_usr" in step for step in plans
        )
