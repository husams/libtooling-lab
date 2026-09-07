import shutil
import sqlite3
import subprocess
import sys
from pathlib import Path

import pytest
from support.facts_data import add_facts
from support.project_data import add_project


def _schema(name: str) -> str:
    return (Path(__file__).parent / "fixtures" / name).read_text(encoding="utf-8")


@pytest.fixture
def paired_databases(tmp_path: Path) -> tuple[Path, Path]:
    facts = tmp_path / "facts.sqlite"
    project = tmp_path / "project.sqlite"
    with sqlite3.connect(facts) as db:
        db.executescript(_schema("facts_schema.sql"))
        add_facts(db)
    with sqlite3.connect(project) as db:
        db.executescript(_schema("project_schema.sql"))
        add_project(db, tmp_path / "checkout λ with spaces")
    return facts, project


@pytest.fixture
def native_schema12_pair(tmp_path: Path) -> tuple[Path, Path]:
    source_root = Path(__file__).parent / "fixtures" / "native"
    root = tmp_path / "native-schema12"
    root.mkdir()
    for name in ("api.hpp", "source.cpp", "generate.py"):
        shutil.copy2(source_root / name, root / name)
    facts_tool = shutil.which("facts-tool") or "/Users/husam/.local/bin/facts-tool"
    compiler = "/opt/homebrew/opt/llvm/bin/clang++"
    if not Path(facts_tool).exists() or not Path(compiler).exists():
        pytest.skip("native facts-tool and Homebrew LLVM are required")
    subprocess.run(
        [sys.executable, str(root / "generate.py"), facts_tool, compiler], check=True
    )
    facts, project = root / "facts.sqlite", root / "project.sqlite"
    subprocess.run(
        [
            facts_tool,
            "analyse",
            "call-graph",
            "-v",
            "0",
            "-f",
            str(facts),
            "-c",
            str(project),
            "--function",
            "app::run",
        ],
        check=True,
    )
    return facts, project
