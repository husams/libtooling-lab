import sqlite3
from typing import Any

from .callgraph_outcomes import count
from .callgraph_page import CallGraphPage


def make_page(
    values: tuple[Any, ...],
    facts: sqlite3.Connection,
    table: str,
    run_id: int,
    start: int,
    bound: int,
    rows: list[sqlite3.Row],
) -> CallGraphPage[Any]:
    cursor = start + bound if len(rows) > bound else None
    total = count(facts, table, run_id)
    return CallGraphPage(values, total, cursor)
