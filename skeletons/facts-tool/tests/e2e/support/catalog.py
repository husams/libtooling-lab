from __future__ import annotations

import shlex
import sqlite3
from pathlib import Path
from dataclasses import dataclass, field
from support.database import file_snapshot, require
from support.scenario import FactsToolContext


@dataclass
class Catalog:
    context: FactsToolContext
    checkout: Path
    second: Path
    external: Path
    sources: dict[Path, bytes] = field(default_factory=dict)
    before: dict[str, list[tuple]] = field(default_factory=dict)
    facts_before: bytes = b""
    resolved_before: list[tuple] = field(default_factory=list)
    stdout: str = ""
    configuration_before: bytes = b""

    def rows(self, sql: str, parameters: tuple = ()) -> list[tuple]:
        # A new read-only connection proves committed state and cannot create a
        # missing database or repair the behavior that the command should supply.
        uri = self.context.files_database_path.as_uri() + "?mode=ro"
        with sqlite3.connect(uri, uri=True) as connection:
            return connection.execute(sql, parameters).fetchall()

    def snapshot(self) -> dict[str, list[tuple]]:
        return {
            table: self.rows(f'SELECT * FROM "{table}" ORDER BY id')
            for table in ("repository", "clone", "component", "directory", "file")
        }

    def remember(self) -> None:
        self.before = self.snapshot()
        self.resolved_before = file_snapshot(self.context.files_database_path)
        self.configuration_before = self.context.files_database_path.read_bytes()

    def run(self, command: str) -> None:
        replacements = {
            "{second-clone}": str(self.second),
            "{external-root}": str(self.external),
            "{core-root}": str(self.checkout / "core"),
            "{checkout}": str(self.checkout),
            "{neighbor-root}": str(self.checkout / "neighbor"),
            "{new-component}": str(self.checkout / "extension"),
            "{missing-path}": str(self.checkout / "missing"),
            "{core-id}": str(self.core_id),
            "{deep-id}": str(self.deep_id),
            "{manual-file}": str(self.checkout / "core/src/deep/manual.cpp"),
            "{nested-file}": str(self.checkout / "core/src/newdir/one.cpp"),
            "{outside-file}": str(self.external / "outside.cpp"),
            "{compiler}": str(self.context.compiler),
            "{working-directory}": str(self.checkout / "core/src"),
        }
        # Split before substitution so a path containing spaces stays one argv.
        arguments = [replacements.get(token, token) for token in shlex.split(command)]
        result = self.context._run(
            [str(self.context.facts_tool), *arguments,
             "--conf", str(self.context.files_database_path)]
        )
        self.stdout = result.stdout
        with (self.context.run_root_path / "catalog-commands.log").open("a") as log:
            log.write(f"{shlex.join(arguments)}\nexit={result.returncode}\n"
                      f"{result.stdout}{result.stderr}\n")
        require(self.context.facts_database_path.read_bytes() == self.facts_before,
                "a catalog command modified the separate extracted-facts database")
        for source, content in self.sources.items():
            require(source.is_file() and source.read_bytes() == content,
                    f"a catalog command changed or deleted checkout source {source}")

    def run_symbol(self, command: str) -> None:
        arguments = shlex.split(command)
        result = self.context._run(
            [str(self.context.facts_tool), "symbol", *arguments,
             "--facts", str(self.context.facts_database_path)]
        )
        self.stdout = result.stdout
        require(self.context.files_database_path.read_bytes() ==
                self.configuration_before,
                "a symbol command modified the project configuration")
        require(self.context.facts_database_path.read_bytes() == self.facts_before,
                "a symbol command modified the facts database")

    @property
    def core_id(self) -> int:
        return next(row[0] for row in self.before["component"] if row[1] == "core")

    @property
    def deep_id(self) -> int:
        return next(row[0] for row in self.before["directory"]
                    if row[1:3] == (self.core_id, "src/deep"))
