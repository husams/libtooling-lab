from __future__ import annotations

import subprocess
from pathlib import Path


def _cli(tool: str, args: list[str], cwd: Path) -> str:
    result = subprocess.run(
        [tool, *args], cwd=cwd, check=True, capture_output=True, text=True
    )
    return result.stdout


def run_native(tool: str, facts: Path, project: Path) -> list[str]:
    root = facts.parent
    outputs = [_cli(tool, ["config", "show"], root)]
    outputs.append(
        _cli(
            tool,
            [
                "match",
                "-v",
                "0",
                "--conf",
                str(project),
                "--facts",
                str(facts),
                "--matcher",
                'functionDecl(hasName("app::run")).bind("symbol")',
                "source.cpp",
            ],
            root,
        )
    )
    found = _cli(
        tool,
        [
            "symbol",
            "find",
            "-v",
            "0",
            "--conf",
            str(project),
            "--facts",
            str(facts),
            "--name",
            "app::run",
            "--format",
            "json",
        ],
        root,
    )
    assert "app::run" in found
    outputs.append(found)
    outputs.append(
        _cli(
            tool,
            [
                "analyse",
                "call-graph",
                "-v",
                "0",
                "--facts",
                str(facts),
                "--conf",
                str(project),
                "--function",
                "app::run",
            ],
            root,
        )
    )
    return outputs
