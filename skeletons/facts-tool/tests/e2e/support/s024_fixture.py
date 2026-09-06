from __future__ import annotations

import json
import sqlite3
import subprocess

from support.database import require
from support.scenario import FactsToolContext


def run(*command: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True, check=False)


def prepare(context: FactsToolContext) -> None:
    context.prepare()
    root = context.fixture_root.parents[1] / "e2e" / "fixtures" / "s024"
    sources = sorted(root.glob("*/*.cpp"))
    commands = [
        {"directory": str(root), "file": str(source), "arguments":
         [str(context.compiler), "-std=c++23", f"-I{root}", "-c", str(source)]}
        for source in sources
    ]
    (context.run_root_path / "compile_commands.json").write_text(
        json.dumps(commands), encoding="utf-8")
    components = sum(
        (["--component", f"{source.parent.name}={source.parent}"]
         for source in sources), [])
    imported = run(str(context.facts_tool), "import", "-v", "0", "-c",
                   str(context.files_database_path), "-p",
                   str(context.run_root_path), *components)
    require(imported.returncode == 0, imported.stdout + imported.stderr)
    extracted = run(str(context.facts_tool), "extract", "-v", "0", "-c",
                    str(context.files_database_path), "-o",
                    str(context.facts_database_path),
                    *(str(path) for path in sources))
    require(extracted.returncode == 0, extracted.stdout + extracted.stderr)
    with sqlite3.connect(context.files_database_path) as database:
        database.execute(
            "UPDATE file SET indexed=1,indexed_at='2026-09-06T00:00:00Z' "
            "WHERE driver IS NOT NULL")
