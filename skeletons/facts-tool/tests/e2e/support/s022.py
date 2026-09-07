from __future__ import annotations

import subprocess

from support.callgraph_run import completion, run_row, run_graph
from support.callgraph_run_rows import edges as run_edges
from support.database import file_snapshot, query, require
from support.scenario import FactsToolContext

RELATION_KIND = {1: "Calls", 18: "DispatchCalls"}
CERTAINTY = {1: "exact", 2: "possible"}
CONSTRUCTOR_KIND = 23
DESTRUCTOR_KIND = 24


def run(arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(arguments, capture_output=True, text=True, check=False)


def _semantic_kind(relation_kind: str, target_name: str, target_kind: int | None) -> str | None:
    if target_kind == CONSTRUCTOR_KIND:
        return "constructor"
    if target_kind == DESTRUCTOR_KIND:
        return "destructor"
    if "::<lambda@" in target_name:
        return "lambda"
    if relation_kind == "DispatchCalls":
        return "virtual_dispatch"
    return None


def graph(context: FactsToolContext, root: str) -> dict:
    """Persist an `analyse call-graph` run for `root` and enrich its edges."""
    result = run_graph(context, "--function", root)
    run_id, status = completion(result)
    require(status in {"complete", "truncated"}, f"{status}: {result.stderr}")
    facts = context.facts_database_path
    paths = dict(file_snapshot(context.files_database_path))
    raw = run_edges(facts, run_id)
    symbol_ids = {edge["source_id"] for edge in raw} | {edge["target_id"] for edge in raw}
    kinds = {}
    if symbol_ids:
        placeholders = ",".join("?" * len(symbol_ids))
        kinds = dict(query(facts, f"SELECT id,kind FROM symbol WHERE id IN ({placeholders})",
                            tuple(symbol_ids)))
    enriched = [_enrich(facts, paths, kinds, edge) for edge in raw]
    return {"run_id": run_id, "row": run_row(facts, run_id), "edges": enriched}


def _enrich(facts, paths: dict, kinds: dict, edge: dict) -> dict:
    site = query(facts, "SELECT line,col,receiver_type_id,certainty FROM relation_site WHERE "
                 "source_id=? AND destination_id=? AND kind=? AND position=? AND file_id=? "
                 "AND offset=?", (edge["source_id"], edge["target_id"], edge["kind"],
                                  edge["position"], edge["file_id"], edge["offset"]))
    require(len(site) == 1, str(edge))
    line, col, receiver_type_id, certainty = site[0]
    implicit = query(facts, "SELECT is_implicit FROM relation WHERE source_id=? AND "
                     "destination_id=? AND kind=? AND position=?",
                     (edge["source_id"], edge["target_id"], edge["kind"], edge["position"]))
    require(len(implicit) == 1, str(edge))
    relation_kind = RELATION_KIND[edge["kind"]]
    receiver = None
    if receiver_type_id:
        rows = query(facts, "SELECT qualified_name FROM symbol WHERE id=?", (receiver_type_id,))
        receiver = rows[0][0] if rows else None
    return {**edge, "relation_kind": relation_kind, "implicit": bool(implicit[0][0]),
            "certainty": CERTAINTY.get(certainty), "receiver_type_id": receiver_type_id,
            "receiver": receiver, "location": {"path": paths.get(edge["file_id"]),
                                                "line": line, "col": col},
            "semantic_kind": _semantic_kind(relation_kind, edge["target"],
                                             kinds.get(edge["target_id"]))}


def named_edges(document: dict) -> list[tuple[dict, str]]:
    return [(edge, edge["target"]) for edge in document["edges"]]


def definition_available(facts, symbol_id: int) -> bool:
    rows = query(facts, "SELECT 1 FROM definition WHERE symbol_id=?", (symbol_id,))
    return bool(rows)


def source_path(paths: dict, symbol_id: int) -> str | None:
    return paths.get(symbol_id >> 32)


def definition_path(facts, paths: dict, symbol_id: int) -> str | None:
    rows = query(facts, "SELECT file_id FROM definition WHERE symbol_id=?", (symbol_id,))
    return paths.get(rows[0][0]) if rows else None
