import sqlite3
from typing import cast

from .callgraph_build_result import make_run
from .callgraph_db import edges, frontier, recovery, roots, sites, targets
from .callgraph_decode import edge, root, symbol, target
from .callgraph_frontier import decode as decode_frontier
from .callgraph_models import CallGraphSymbol
from .callgraph_outcomes import path_flags
from .callgraph_page_build import make_page
from .callgraph_recovery import decode as decode_recovery
from .callgraph_result import CallGraphRun
from .paths import FileResolver
from .provenance import PairProvenance


def decode_run(
    facts: sqlite3.Connection,
    files: FileResolver,
    provenance: PairProvenance,
    row: sqlite3.Row,
    bound: int,
    cursors: dict[str, int],
) -> CallGraphRun:
    run_id = int(row["run_id"])

    def symbols(value: object) -> CallGraphSymbol:
        return symbol(facts, files, int(cast(int, value)))

    offsets = {
        name: cursors.get(name, 0)
        for name in ("roots", "targets", "edges", "frontier", "recovery")
    }
    root_rows = roots(facts, run_id, bound + 1, offsets["roots"])
    target_rows = targets(facts, run_id, bound + 1, offsets["targets"])
    edge_rows = edges(facts, run_id, bound + 1, offsets["edges"])
    frontier_rows = frontier(facts, run_id, bound + 1, offsets["frontier"])
    recovery_rows = recovery(facts, run_id, bound + 1, offsets["recovery"])
    roots_out = tuple(
        root(item, symbols(item["symbol_id"])) for item in root_rows[:bound]
    )
    targets_out = tuple(
        target(item, symbols(item["symbol_id"])) for item in target_rows[:bound]
    )
    edge_out = tuple(
        edge(
            item,
            symbols(item["source_id"]),
            symbols(item["destination_id"]),
            sites(facts, item),
            files,
        )
        for item in edge_rows[:bound]
    )
    frontier_out = tuple(
        decode_frontier(item, symbols(item["symbol_id"]))
        for item in frontier_rows[:bound]
    )
    recovery_out = tuple(decode_recovery(item, files) for item in recovery_rows[:bound])
    components = tuple(filter(None, str(row["components"] or "").split(",")))
    target_exists, target_was_reached, self_path_exists = path_flags(facts, run_id)
    specs = (
        (roots_out, "callgraph_run_root", "roots", root_rows),
        (targets_out, "callgraph_run_target", "targets", target_rows),
        (edge_out, "callgraph_run_edge", "edges", edge_rows),
        (frontier_out, "callgraph_run_frontier", "frontier", frontier_rows),
        (recovery_out, "callgraph_run_recovery", "recovery", recovery_rows),
    )
    pages = tuple(
        make_page(tuple(values), facts, table, run_id, offsets[name], bound, rows)
        for values, table, name, rows in specs
    )
    return make_run(
        row,
        components,
        pages,
        provenance,
        (target_exists, target_was_reached, self_path_exists),
    )
