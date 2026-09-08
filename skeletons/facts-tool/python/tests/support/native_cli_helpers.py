from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path


def tool() -> Path:
    configured = os.environ.get("FACTS_TOOL_NATIVE")
    root = Path(__file__).parents[3]
    candidates = (root / "build-s032" / "facts-tool", root / "build" / "facts-tool")
    candidate = next((path for path in candidates if path.exists()), None)
    return (
        Path(configured)
        if configured
        else (candidate or Path(shutil.which("facts-tool") or "facts-tool"))
    )


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
