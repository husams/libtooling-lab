from __future__ import annotations

import os
import subprocess
from pathlib import Path


def tool() -> Path:
    configured = os.environ.get("FACTS_TOOL_NATIVE")
    if not configured:
        raise RuntimeError("FACTS_TOOL_NATIVE must name the current native executable")
    executable = Path(configured)
    if not executable.is_file():
        raise RuntimeError(f"FACTS_TOOL_NATIVE is not a file: {executable}")
    return executable


def call(executable: Path, args: list[str], root: Path) -> str:
    result = subprocess.run(
        [str(executable), *args], cwd=root, check=True, capture_output=True, text=True
    )
    return result.stdout + result.stderr


def match(
    executable: Path, project: Path, facts: Path, root: Path, expression: str
) -> str:
    return call(
        executable,
        [
            "match",
            "-v",
            "0",
            "--conf",
            str(project),
            "--facts",
            str(facts),
            "--matcher",
            expression,
            "--capture-source",
            "source.cpp",
        ],
        root,
    )
