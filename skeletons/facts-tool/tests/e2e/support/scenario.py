from __future__ import annotations

import concurrent.futures
import json
import os
import sqlite3
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
    run_root: Optional[Path] = None
    facts_database: Optional[Path] = None
    files_database: Optional[Path] = None
    initial_files: list[tuple] = field(default_factory=list)
    initial_symbols: list[tuple] = field(default_factory=list)
    last_returncode: Optional[int] = None
    last_output: str = ""
    prepared: bool = False
    extracted: bool = False

    @classmethod
    def create(
        cls,
        facts_tool: Path,
        fixture_root: Path,
        compiler: Path,
        output_root: Path,
    ) -> FactsToolContext:
        fixture_root = fixture_root.resolve(strict=True)
        sources = tuple(
            (fixture_root / name).resolve(strict=True)
            for name in ("one.cpp", "two.cpp")
        )
        return cls(
            facts_tool=facts_tool.resolve(strict=True),
            fixture_root=fixture_root,
            compiler=compiler.resolve(strict=True),
            output_root=output_root.resolve(),
            sources=(sources[0], sources[1]),
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
        self.run_import()
        self.run_tool()
        self.initial_files = file_snapshot(self.files_database_path)
        self.initial_symbols = symbol_snapshot(self.facts_database_path)
        self.extracted = True

    def run_tool(self) -> str:
        completed = self._run(self.tool_command())
        require(
            completed.returncode == 0,
            f"facts-tool exited with {completed.returncode}:\n{self.last_output}",
        )
        require(
            "symbol(s) recorded" in self.last_output,
            f"missing extraction summary:\n{self.last_output}",
        )
        return self.last_output

    def run_import(self, sources: tuple[Path, ...] | None = None) -> str:
        completed = self._run(self.import_command(sources))
        require(
            completed.returncode == 0,
            f"facts-tool import exited with {completed.returncode}:\n{self.last_output}",
        )
        return self.last_output

    def run_concurrently(self) -> list[str]:
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
            return list(executor.map(lambda _: self.run_tool(), range(2)))

    def rerun_from_stored_compile_options(self) -> None:
        self.extract()
        self._store_compile_options(self._compile_options())
        self._remove_compilation_database()
        self._select_facts_database("stored-facts.sqlite")
        self._run(self.stored_tool_command())

    def rerun_from_labeled_stored_compile_options(self) -> None:
        self.extract()
        self._store_compile_options(
            [
                "-std=c++23",
                "-I<fixture>",
                "-isystem",
                str(self.fixture_root / "system"),
            ]
        )
        with sqlite3.connect(self.files_database_path) as connection:
            connection.execute(
                "CREATE TABLE label(name TEXT PRIMARY KEY, path TEXT NOT NULL)"
            )
            connection.execute(
                "INSERT INTO label(name,path) VALUES('fixture',?)",
                (str(self.fixture_root),),
            )
        self._remove_compilation_database()
        self._select_facts_database("labeled-stored-facts.sqlite")
        self._run(self.stored_tool_command())

    def rerun_with_json_over_malformed_stored_options(self) -> None:
        self.extract()
        self._set_raw_compile_options('{"invalid":true}')
        self._select_facts_database("json-precedence-facts.sqlite")
        self.run_import()
        self._run(self.tool_command())

    def run_with_malformed_stored_options(self) -> None:
        self.extract()
        self._set_raw_compile_options('{"invalid":true}')
        self._remove_compilation_database()
        self._select_facts_database("malformed-stored-facts.sqlite")
        self._run(self.stored_tool_command())

    def run_with_missing_stored_command(self, filename: str) -> None:
        self.extract()
        self._store_compile_options(self._compile_options())
        with sqlite3.connect(self.files_database_path) as connection:
            connection.execute(
                "UPDATE file SET compile_options=NULL,driver=NULL WHERE name=?",
                (filename,),
            )
        self._remove_compilation_database()
        self._select_facts_database("missing-stored-command-facts.sqlite")
        self._run(self.stored_tool_command())

    def run_with_unrelated_missing_include_root(self) -> None:
        self._run_with_missing_include_root("two.cpp")

    def run_with_missing_include_root_on_selected_source(self) -> None:
        self._run_with_missing_include_root("one.cpp")

    def _run_with_missing_include_root(self, filename: str) -> None:
        self.extract()
        self._store_compile_options(self._compile_options())
        require(
            not self.missing_include_root.exists(),
            "the missing include root must not exist",
        )
        with sqlite3.connect(self.files_database_path) as connection:
            connection.execute(
                "UPDATE file SET driver=?,compile_options=? WHERE name=?",
                (
                    str(self.compiler),
                    json.dumps(
                        self._compile_options() + [f"-I{self.missing_include_root}"]
                    ),
                    filename,
                ),
            )
        self._remove_compilation_database()
        self._select_facts_database(f"missing-include-root-{filename}.sqlite")
        self._run(self._tool_command((self.sources[0],)))

    @property
    def missing_include_root(self) -> Path:
        return self.run_root_path / "missing-include-root"

    def run_with_deprecated_files_out_option(self) -> None:
        self.prepare()
        self._select_facts_database("deprecated-files-out-facts.sqlite")
        self._run(
            [
                str(self.facts_tool),
                "extract",
                "--output",
                str(self.facts_database_path),
                "--conf",
                str(self.files_database_path),
                "--files-out",
                *(str(source) for source in self.sources),
            ]
        )

    def force_relation_persistence_failure(self) -> None:
        self.extract()
        with sqlite3.connect(self.facts_database_path) as connection:
            connection.execute(
                "CREATE TRIGGER force_relation_failure "
                "BEFORE INSERT ON relation BEGIN "
                "SELECT RAISE(ABORT, 'forced relation persistence failure'); "
                "END"
            )
        self._run(self.tool_command())

    def force_field_relation_persistence_failure(self) -> None:
        self.extract()
        with sqlite3.connect(self.facts_database_path) as connection:
            connection.execute(
                "CREATE TRIGGER force_field_relation_failure "
                "BEFORE INSERT ON relation WHEN NEW.kind=8 BEGIN "
                "SELECT RAISE(ABORT, 'forced field relation persistence failure'); "
                "END"
            )
        self._run(self.tool_command())

    def force_second_inheritance_relation_failure(self) -> None:
        self.extract()
        with sqlite3.connect(self.facts_database_path) as connection:
            connection.execute(
                "CREATE TRIGGER force_second_inheritance_failure "
                "BEFORE INSERT ON relation "
                "WHEN NEW.kind=2 AND NEW.position=1 BEGIN "
                "SELECT RAISE(ABORT, 'forced second inheritance failure'); "
                "END"
            )
        self._run(self.tool_command())

    def run_dependent_base_fixture(self) -> None:
        self.prepare()
        source = (self.fixture_root / "dependent_base.cpp").resolve(strict=True)
        self.facts_database = self.run_root_path / "dependent-facts.sqlite"
        self.files_database = self.run_root_path / "dependent-files.sqlite"
        self._write_compilation_database((source,))
        self.run_import((source,))
        self._run(self._tool_command((source,)))

    def stored_tool_command(self) -> list[str]:
        return [
            str(self.facts_tool),
            "extract",
            "--output",
            str(self.facts_database_path),
            "--conf",
            str(self.files_database_path),
            *(str(source) for source in self.sources),
        ]

    def import_command(self, sources: tuple[Path, ...] | None = None) -> list[str]:
        requested_sources = self.sources if sources is None else sources
        return [
            str(self.facts_tool),
            "import",
            "--conf",
            str(self.files_database_path),
            "--compilation-database",
            str(self.run_root_path),
            *(str(source) for source in requested_sources),
        ]

    def _run(self, command: list[str]) -> subprocess.CompletedProcess[str]:
        completed = subprocess.run(
            command, capture_output=True, text=True, check=False
        )
        self.last_returncode = completed.returncode
        self.last_output = completed.stdout + completed.stderr
        return completed

    def _store_compile_options(self, options: list[str]) -> None:
        self._set_raw_compile_options(json.dumps(options))

    def _compile_options(self) -> list[str]:
        return [
            "-std=c++23",
            f"-I{self.fixture_root}",
            "-isystem",
            str(self.fixture_root / "system"),
        ]

    def _set_raw_compile_options(self, options: str) -> None:
        with sqlite3.connect(self.files_database_path) as connection:
            connection.execute(
                "UPDATE file SET driver=?,compile_options=? "
                "WHERE name IN ('one.cpp','two.cpp')",
                (str(self.compiler), options),
            )

    def _remove_compilation_database(self) -> None:
        (self.run_root_path / "compile_commands.json").unlink()

    def _select_facts_database(self, filename: str) -> None:
        self.facts_database = self.run_root_path / filename

    def tool_command(self) -> list[str]:
        return self._tool_command(self.sources)

    def _tool_command(self, sources: tuple[Path, ...]) -> list[str]:
        return [
            str(self.facts_tool),
            "extract",
            "--output",
            str(self.facts_database_path),
            "--conf",
            str(self.files_database_path),
            *(str(source) for source in sources),
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

    def _write_compilation_database(
        self, sources: Optional[tuple[Path, ...]] = None
    ) -> None:
        selected_sources = sources or self.sources
        commands = [
            {
                "directory": str(self.fixture_root),
                "file": str(source),
                "arguments": [
                    str(self.compiler),
                    "-std=c++23",
                    f"-I{self.fixture_root}",
                    "-isystem",
                    str(self.fixture_root / "system"),
                    "-c",
                    str(source),
                ],
            }
            for source in selected_sources
        ]
        (self.run_root_path / "compile_commands.json").write_text(
            json.dumps(commands, indent=2) + "\n", encoding="utf-8"
        )
