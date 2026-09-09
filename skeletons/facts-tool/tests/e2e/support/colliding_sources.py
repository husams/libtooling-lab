"""Fixture for B-043: two components spelling the same relative source path.

The files are generated per scenario rather than committed so the large
component stays comfortably above the mmap threshold (4 pages) and both
spellings are byte-for-byte the same relative path in the compilation
database. Sizes are deliberately asymmetric in the same direction for the
source and the header: component-a is the large one for both, so a
large-then-small import order collides on both spellings.
"""
from __future__ import annotations

import json
import subprocess
from pathlib import Path

from support.database import file_snapshot, require
from support.scenario import FactsToolContext

RELATIVE_SOURCE = "src/generated/Same.cpp"
RELATIVE_HEADER = "include/common.h"
COMPONENTS = ("component-a", "component-b")
NULL_CHARACTER_DIAGNOSTIC = "null character ignored"


def _padding(lines: int, width: int) -> str:
    return "".join(f"// {i:06d} {'a' * width}\n" for i in range(lines))


def write_component(context: FactsToolContext, component: str, large: bool) -> Path:
    context.prepare()
    root = context.run_root_path / component
    source = root / RELATIVE_SOURCE
    header = root / RELATIVE_HEADER
    source.parent.mkdir(parents=True, exist_ok=True)
    header.parent.mkdir(parents=True, exist_ok=True)
    function = "large_fn" if large else "small_fn"
    body = f'#include "common.h"\nint {function}() {{ return COMMON_VALUE; }}\n'
    body += _padding(1000, 70) if large else _padding(40, 4)
    source.write_text(body, encoding="utf-8")
    return source


def write_headers(context: FactsToolContext) -> None:
    for component, value, lines in ((COMPONENTS[0], 1, 300), (COMPONENTS[1], 2, 2)):
        header = context.run_root_path / component / RELATIVE_HEADER
        header.write_text(
            f"#pragma once\n#define COMMON_VALUE {value}\n" + _padding(lines, 60),
            encoding="utf-8",
        )
    sizes = {c: (context.run_root_path / c / RELATIVE_HEADER).stat().st_size for c in COMPONENTS}
    require(
        sizes[COMPONENTS[0]] > 16384 > sizes[COMPONENTS[1]],
        f"header sizes must straddle the mmap threshold, got {sizes}",
    )


def write_compilation_database(context: FactsToolContext) -> None:
    commands = [
        {
            "directory": str(context.run_root_path / component),
            "file": RELATIVE_SOURCE,
            "arguments": [
                str(context.compiler),
                "-std=c++17",
                "-Iinclude",
                "-c",
                RELATIVE_SOURCE,
                "-o",
                "Same.o",
            ],
        }
        for component in COMPONENTS
    ]
    (context.run_root_path / "compile_commands.json").write_text(
        json.dumps(commands, indent=2) + "\n", encoding="utf-8"
    )
    sizes = {c: (context.run_root_path / c / RELATIVE_SOURCE).stat().st_size for c in COMPONENTS}
    require(
        sizes[COMPONENTS[0]] > 65536 > sizes[COMPONENTS[1]],
        f"source sizes must straddle 64 KiB, got {sizes}",
    )


def break_component_include(context: FactsToolContext, relative: str) -> None:
    source = context.run_root_path / relative
    source.write_text(
        '#include "definitely_missing_header.hpp"\n' + source.read_text(encoding="utf-8"),
        encoding="utf-8",
    )


def run_import(context: FactsToolContext, *sources: str) -> subprocess.CompletedProcess[str]:
    """Run import from the fixture root with the sources spelled relatively.

    This mirrors the reported invocation (`facts-tool import -p . X Y`), so
    the selected sources and the compile commands both stay relative. The
    paired facts store is named so a repeated import of the same project is
    allowed to mutate it.
    """
    command = [
        str(context.facts_tool),
        "import",
        "-p",
        ".",
        "--conf",
        str(context.files_database_path),
        "--facts",
        str(context.facts_database_path),
        "-v",
        "3",
        *sources,
    ]
    completed = subprocess.run(
        command,
        capture_output=True,
        text=True,
        check=False,
        cwd=context.run_root_path,
    )
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr
    context.import_output = context.last_output
    return completed


def registered_paths(context: FactsToolContext) -> list[str]:
    return [path for (_, path) in file_snapshot(context.files_database_path)]


def absolute(context: FactsToolContext, relative: str) -> str:
    return str((context.run_root_path / relative).resolve())
