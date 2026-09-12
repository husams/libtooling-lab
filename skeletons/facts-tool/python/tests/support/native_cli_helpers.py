from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path


def tool() -> Path:
    configured = os.environ.get("FACTS_TOOL_NATIVE")
    if configured:
        executable = Path(configured)
        if not executable.is_file():
            raise RuntimeError(f"FACTS_TOOL_NATIVE is not a file: {executable}")
        return executable
    current = Path(__file__).parents[3] / "build" / "facts-tool"
    if current.is_file():
        return current
    discovered = shutil.which("facts-tool")
    if discovered:
        return Path(discovered)
    raise RuntimeError("current native facts-tool executable was not found")


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
