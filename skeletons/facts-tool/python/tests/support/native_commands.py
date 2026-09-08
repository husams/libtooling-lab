from __future__ import annotations

import json
import re
import shutil
from pathlib import Path

from support.native_cli_helpers import call, match, tool


def prepare_native_pair(root: Path) -> tuple[Path, Path, Path, int, int]:
    fixture = Path(__file__).parents[1] / "fixtures" / "native"
    root.mkdir(parents=True)
    for name in ("api.hpp", "source.cpp"):
        shutil.copy2(fixture / name, root / name)
    source = root / "source.cpp"
    source.write_text(
        source.read_text(encoding="utf-8").replace(
            "return save() + save() + box.value;",
            "box.value = 7;\n  return save() + save() + box.value;",
        ),
        encoding="utf-8",
    )
    project, facts = root / "project.sqlite", root / "facts.sqlite"
    compiler = Path("/opt/homebrew/opt/llvm/bin/clang++")
    commands = [
        {
            "directory": str(root),
            "file": str(source),
            "arguments": [str(compiler), "-std=c++20", "-c", str(source)],
        }
    ]
    (root / "compile_commands.json").write_text(json.dumps(commands), encoding="utf-8")
    executable = tool()
    outputs = [
        call(
            executable,
            ["import", "--conf", str(project), "--facts", str(facts), "-p", str(root)],
            root,
        ),
        call(
            executable,
            ["extract", "--conf", str(project), "--output", str(facts)],
            root,
        ),
        match(executable, project, facts, root, 'memberExpr().bind("expression")'),
    ]
    for expression in (
        'functionDecl(hasName("app::run")).bind("symbol")',
        'cxxMethodDecl(hasName("flush")).bind("symbol")',
        'cxxRecordDecl(hasName("Box")).bind("symbol")',
    ):
        outputs.append(match(executable, project, facts, root, expression))
    graph = call(
        executable,
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
    outputs.append(graph)
    match_id = re.search(r"call graph run (\d+) complete", graph)
    if match_id is None:
        raise AssertionError(f"native call graph did not report a run id: {graph}")
    return facts, project, source, int(match_id.group(1)), sum(map(len, outputs))
