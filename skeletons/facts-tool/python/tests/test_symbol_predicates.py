import sqlite3

import pytest

from facts_tool.catalog_kinds import symbol_kind
from facts_tool.ids import SymbolId
from facts_tool.predicate_eval import evaluate
from facts_tool.queryplan import all_of, any_of, eq, in_list, ne, not_
from facts_tool.symbol_predicates import compile_symbol_predicate


@pytest.mark.parametrize("field", ["id", "usr", "qualified_name", "kind", "kind_id"])
@pytest.mark.parametrize(
    "value",
    [
        None,
        0,
        1,
        True,
        13.0,
        1.5,
        "1",
        "function",
        "kind_42",
        "kind_-1",
        "None",
        "kind_013",
        "x' OR 1=1 --",
        (1 << 64) - 1,
        -1,
        float("inf"),
        b"1",
    ],
)
@pytest.mark.parametrize("constructor", [eq, ne, lambda f, v: in_list(f, [v])])
def test_sql_predicates_preserve_python_equality(field, value, constructor):
    pred = constructor(field, value)
    compiled = compile_symbol_predicate(pred)
    assert compiled is not None
    clause, params = compiled
    with sqlite3.connect(":memory:") as db:
        db.row_factory = sqlite3.Row
        db.execute(
            "CREATE TABLE symbol(id INTEGER PRIMARY KEY, usr TEXT, "
            "qualified_name TEXT, kind INTEGER)"
        )
        db.executemany(
            "INSERT INTO symbol VALUES(?,?,?,?)",
            [(-1, "1", "1", 13), (0, None, None, -1), (1, "a", "a", 42)],
        )
        rows = []
        for row in db.execute("SELECT * FROM symbol ORDER BY id"):
            rows.append(
                dict(row)
                | {
                    "id": SymbolId.unpack(row["id"]).packed,
                    "kind_id": row["kind"],
                    "kind": symbol_kind(row["kind"]),
                }
            )
        expected = [
            row["id"] for row in rows if evaluate(pred, row, lambda *_: [], 10) is True
        ]
        actual = [
            SymbolId.unpack(row[0]).packed
            for row in db.execute(
                f"SELECT s.id FROM symbol s WHERE {clause} ORDER BY s.id", params
            )
        ]
        assert actual == expected


@pytest.mark.parametrize(
    "pred",
    [
        all_of([]),
        any_of([]),
        in_list("kind", []),
        all_of([eq("kind", "function"), ne("usr", None)]),
        not_(any_of([eq("id", 1), eq("kind_id", 42)])),
    ],
)
def test_compound_predicates_compile(pred):
    assert compile_symbol_predicate(pred) is not None


@pytest.mark.parametrize(
    "pred",
    [
        eq("name", "foo"),
        in_list("missing", []),
        eq("usr", object()),
        all_of([eq("kind", "function"), eq("file", "foo.cpp")]),
        in_list("id", list(range(501))),
    ],
)
def test_unsupported_predicates_use_python_fallback(pred):
    assert compile_symbol_predicate(pred) is None
