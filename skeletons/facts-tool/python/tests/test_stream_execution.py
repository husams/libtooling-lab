import sqlite3
from contextlib import closing

import pytest
from support.symbol_factory import COLUMNS, symbol

from facts_tool import Budgets, open_codebase, view_symbols
from facts_tool import queryplan as qp


@pytest.fixture
def large_cb(paired_databases, monkeypatch):
    facts, project = paired_databases
    with sqlite3.connect(facts) as db:
        db.executemany(
            f"INSERT INTO symbol({','.join(COLUMNS)}) "
            f"VALUES({','.join('?' * len(COLUMNS))})",
            (
                symbol((1 << 32) + n, 1, 13, f"u{n}", f"bulk::f{n}")
                for n in range(1000, 5000)
            ),
        )
    decoded, decode = [], view_symbols._symbol

    def record(row, files):
        decoded.append(row["id"])
        return decode(row, files)

    monkeypatch.setattr(view_symbols, "_symbol", record)
    with open_codebase(facts_db=facts, project_db=project) as cb:
        yield cb, decoded


def test_prefix_decoding_and_cursor_cleanup(large_cb, paired_databases):
    cb, decoded = large_cb
    result = cb.query().nodes().run()
    assert not decoded
    with closing(iter(result)) as rows:
        next(rows)
        assert len(decoded) == 1
        with closing(sqlite3.connect(paired_databases[0], timeout=0)) as writer:
            writer.execute("UPDATE symbol SET line=line+1")
            with pytest.raises(sqlite3.OperationalError, match="locked"):
                writer.commit()
            rows.close()
            writer.commit()


@pytest.mark.parametrize("project", [False, True])
def test_capped_page_cursor_resumes_without_skipping(large_cb, project):
    cb, decoded = large_cb
    query = qp.start(qp.codebase()) | qp.nodes()
    query = query | qp.select(("name",)) if project else query
    first = cb.executor.run(query.plan, result_cap=2).materialize()
    assert first.truncated and first.cursor == str((1 << 32) | 1)
    assert len(decoded) == 3
    second = cb.executor.run(query.plan, result_cap=2, after_id=first.cursor)
    assert next(iter(second))["name"] == "save"
    if project:
        assert first.to_dict()["rows"] == [{"name": "int"}, {"name": "run"}]


@pytest.mark.parametrize(
    "field,value", [("qualified_name", "app::save"), ("name", "save")]
)
def test_filter_respects_source_enumeration_budget(large_cb, field, value):
    cb, _ = large_cb
    cb.executor.budgets = Budgets(enumeration=2)
    query = qp.start(qp.codebase()) | qp.nodes(qp.eq(field, value))
    first = cb.executor.run(query.plan).materialize()
    assert first.values == () and first.truncated
    assert first.cursor == str((1 << 32) | 1)
    second = cb.executor.run(query.plan, after_id=first.cursor)
    assert next(iter(second))["name"] == "save"


@pytest.mark.parametrize("first", [False, True])
def test_first_and_limit_short_circuit_decoding(large_cb, first):
    cb, decoded = large_cb
    query = cb.query().nodes()
    assert query.first() is not None if first else len(query.limit(1).all()) == 1
    assert len(decoded) == 1


@pytest.mark.parametrize("stage", [qp.order_by(("name",)), qp.count()])
def test_sort_and_count_lazy_match_eager(large_cb, stage):
    cb, _ = large_cb
    query = qp.start(qp.codebase()) | qp.nodes(qp.eq("kind", "function"))
    query = query | qp.limit(25) | stage
    lazy = cb.executor.run(query.plan)
    eager = cb.executor.run(query.plan, lazy=False)
    assert lazy.to_dict() == eager.to_dict()


def test_early_limit_preserves_symbol_enumeration_metadata(large_cb):
    cb, decoded = large_cb
    cb.executor.budgets = Budgets(enumeration=2)
    result = cb.query().nodes().limit(1).run().materialize()
    assert len(result.values) == len(decoded) == 1 and result.truncated
    assert result.cursor == str((1 << 32) | 1)
