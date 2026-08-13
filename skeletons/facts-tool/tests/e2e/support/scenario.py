from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import subprocess
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from support.database import file_snapshot, require, symbol_snapshot


@dataclass
class FactsToolContext:
    facts_tool: Path
    fixture_root: Path
    compiler: Path
    output_root: Path
    sources: tuple[Path, Path]
    header: Path
    run_root: Optional[Path] = None
    facts_database: Optional[Path] = None
    files_database: Optional[Path] = None
    initial_files: list[tuple] = field(default_factory=list)
    initial_symbols: list[tuple] = field(default_factory=list)
    prepared: bool = False
    extracted: bool = False
    stability_checked: bool = False

    @classmethod
    def from_args(cls, args: argparse.Namespace) -> FactsToolContext:
        fixture_root = args.fixture_root.resolve(strict=True)
        sources = tuple(
            (fixture_root / name).resolve(strict=True)
            for name in ("one.cpp", "two.cpp")
        )
        return cls(
            facts_tool=args.facts_tool.resolve(strict=True),
            fixture_root=fixture_root,
            compiler=args.compiler.resolve(strict=True),
            output_root=args.output_root.resolve(),
            sources=(sources[0], sources[1]),
            header=(fixture_root / "shared.hpp").resolve(strict=True),
        )

    def prepare(self) -> None:
        if self.prepared:
            return
        self.output_root.mkdir(parents=True, exist_ok=True)
        self.run_root = self.output_root / (f"scenario-{os.getpid()}-{time.time_ns()}")
        self.run_root.mkdir()
        self.facts_database = self.run_root / "facts.sqlite"
        self.files_database = self.run_root / "files.sqlite"
        self._write_compilation_database()
        require(not self.facts_database.exists(), "facts database must start fresh")
        require(not self.files_database.exists(), "files database must start fresh")
        self.prepared = True

    def extract(self) -> None:
        self.prepare()
        if self.extracted:
            return
        self.run_tool()
        self.initial_files = file_snapshot(self.files_database_path)
        self.initial_symbols = symbol_snapshot(self.facts_database_path)
        self.extracted = True

    def run_tool(self) -> str:
        completed = subprocess.run(
            self.tool_command(), capture_output=True, text=True, check=False
        )
        output = completed.stdout + completed.stderr
        require(
            completed.returncode == 0,
            f"facts-tool exited with {completed.returncode}:\n{output}",
        )
        require(
            "symbol(s) recorded" in output,
            f"missing extraction summary:\n{output}",
        )
        return output

    def run_concurrently(self) -> list[str]:
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
            return list(executor.map(lambda _: self.run_tool(), range(2)))

    def tool_command(self) -> list[str]:
        return [
            str(self.facts_tool),
            "-p",
            str(self.run_root_path),
            "--facts-out",
            str(self.facts_database_path),
            "--files-out",
            str(self.files_database_path),
            *(str(source) for source in self.sources),
        ]

    @property
    def run_root_path(self) -> Path:
        require(self.run_root is not None, "scenario is not prepared")
        return self.run_root

    @property
    def facts_database_path(self) -> Path:
        require(self.facts_database is not None, "scenario is not prepared")
        return self.facts_database

    @property
    def files_database_path(self) -> Path:
        require(self.files_database is not None, "scenario is not prepared")
        return self.files_database

    def _write_compilation_database(self) -> None:
        commands = [
            {
                "directory": str(self.fixture_root),
                "file": str(source),
                "arguments": [
                    str(self.compiler),
                    "-std=c++23",
                    f"-I{self.fixture_root}",
                    "-c",
                    str(source),
                ],
            }
            for source in self.sources
        ]
        (self.run_root_path / "compile_commands.json").write_text(
            json.dumps(commands, indent=2) + "\n", encoding="utf-8"
        )
