import sqlite3
from typing import Any

from .callgraph_page import CallGraphPage
from .callgraph_result import CallGraphRun
from .provenance import PairProvenance


def make_run(
    row: sqlite3.Row,
    components: tuple[str, ...],
    pages: tuple[CallGraphPage[Any], ...],
    provenance: PairProvenance,
    flags: tuple[bool, bool, bool],
) -> CallGraphRun:
    return CallGraphRun(
        int(row["run_id"]),
        str(row["created_at"]),
        str(row["project_path"]),
        str(row["facts_path"]),
        str(row["mode"]),
        row["path_mode"],
        str(row["calls_scope"]),
        components,
        row["max_depth"],
        row["max_nodes"],
        row["max_edges"],
        row["time_limit_ms"],
        bool(row["recover_missing"]),
        str(row["status"]),
        row["truncation_reason"],
        row["error"],
        pages[0],
        pages[1],
        pages[2],
        pages[3],
        pages[4],
        provenance,
        flags[0],
        flags[1],
        flags[2],
    )
