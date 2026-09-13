from __future__ import annotations

import json
import shutil
import subprocess
from pathlib import Path

from support.native_cli_helpers import compiler, tool


def prepare_variable_flow(root: Path) -> tuple[Path, Path, Path]:
    fixture = Path(__file__).parents[2] / ".." / "tests" / "fixtures" / "e2e"
    root.mkdir()
    sources = tuple(
        root / name for name in ("variable_flow_root.cpp", "variable_flow_calls.cpp")
    )
    for name, destination in zip((path.name for path in sources), sources, strict=True):
        shutil.copy2(fixture / name, destination)
    target_compiler = compiler()
    commands = [
        {
            "directory": str(root),
            "file": str(source),
            "arguments": [str(target_compiler), "-std=c++23", "-c", str(source)],
        }
        for source in sources
    ]
    (root / "compile_commands.json").write_text(json.dumps(commands), encoding="utf-8")
    project, facts = root / "project.db", root / "facts.db"
    executable = tool()
    _run(
        executable,
        root,
        "import",
        "--conf",
        str(project),
        "--facts",
        str(facts),
        "-p",
        str(root),
    )
    _run(
        executable,
        root,
        "extract",
        "--conf",
        str(project),
        "--output",
        str(facts),
        *(str(source) for source in sources),
    )
    (root / "settings.yaml").write_text(
        f"conf_template: '{project}'\nfacts_template: '{facts}'\n",
        encoding="utf-8",
    )
    return project, facts, executable


def analyse(
    executable: Path,
    root: Path,
    project: Path,
    facts: Path,
    output: Path,
    function: str,
    variable: str,
) -> None:
    _run(
        executable,
        root,
        "analyse",
        "variable-flow",
        "--config",
        str(root / "settings.yaml"),
        "--output",
        str(output),
        "--function",
        function,
        "--variable",
        variable,
    )


def _run(executable: Path, root: Path, *args: str) -> None:
    subprocess.run(
        [str(executable), *args], cwd=root, check=True, capture_output=True, text=True
    )
