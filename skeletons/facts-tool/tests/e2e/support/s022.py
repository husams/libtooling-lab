from __future__ import annotations

import json
import subprocess

from support.database import require
from support.scenario import FactsToolContext


def run(arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(arguments, capture_output=True, text=True, check=False)


def graph(context: FactsToolContext, root: str, view: str = "semantic") -> dict:
    result = run([str(context.facts_tool), "analyse", "call-graph", "-v", "0",
                  "-f", str(context.facts_database_path), "-c",
                  str(context.files_database_path), "--format", "json",
                  "--edges", view, "--function", root])
    require(result.returncode == 0, result.stdout + result.stderr)
    return json.loads(result.stdout)


def named_edges(document: dict) -> list[tuple[dict, str]]:
    names = {node["id"]: node["name"] for node in document["nodes"]}
    return [(edge, names[edge["target_id"]]) for edge in document["edges"]]
