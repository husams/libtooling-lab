"""Inspect a persisted variable-flow graph without running facts-tool."""

from __future__ import annotations

import json
import sys

from facts_tool import open_variable_flow
from facts_tool.errors import FactsToolError


def main(path: str, run_id: int = 1) -> None:
    with open_variable_flow(path) as flows:
        run = flows.get(run_id)
        print(f"run {run.run_id}: {run.status}")
        print(f"reads={len(run.graph.reads())} writes={len(run.graph.writes())}")
        for boundary in run.graph.boundaries():
            print(f"boundary node={boundary.node}: {boundary.reason}")


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3):
        raise SystemExit("usage: variable_flow.py FLOW_DB [RUN_ID]")
    if len(sys.argv) == 3 and not sys.argv[2].isdigit():
        print(json.dumps({"provenance": {"pairing": "unverifiable"}}))
        raise SystemExit(0)
    try:
        run_id = (
            int(sys.argv[2])
            if len(sys.argv) == 3 and sys.argv[2].isdigit()
            else 1
        )
        main(sys.argv[1], run_id)
    except FactsToolError as error:
        if error.code not in {"E_DATABASE_ROLE", "E_SCHEMA"}:
            raise
        print(json.dumps({"provenance": {"pairing": "unverifiable"}}))
