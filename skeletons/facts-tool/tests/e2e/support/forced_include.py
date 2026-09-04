from __future__ import annotations

import json
import shutil
import sqlite3
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path

from support.scenario import FactsToolContext


@dataclass
class ForcedIncludeFixture:
    root: Path
    source: Path
    header: Path | None = None
    conf: Path | None = None

    @classmethod
    def create(
        cls, context: FactsToolContext, local_header: bool, system_header: bool = False
    ) -> ForcedIncludeFixture:
        context.output_root.mkdir(parents=True, exist_ok=True)
        root = Path(tempfile.mkdtemp(prefix="b028-", dir=context.output_root)).resolve()
        shutil.copytree(context.fixture_root / "forced-include", root,
                        dirs_exist_ok=True)
        source = root / "optional.cpp"
        header = None
        if local_header:
            header = root / ("system" if system_header else "include") / "forced.hpp"
            source = root / "paths.cpp"
        return cls(root, source, header)

    def run_fixed_import(self, context: FactsToolContext, options: list[str], conf: Path) -> None:
        self.conf = conf
        self._record(context, subprocess.run(
            [str(context.facts_tool), "import", "--conf", str(conf), str(self.source),
             *sum((["--extra-arg", option] for option in options), [])],
            capture_output=True, text=True, check=False, cwd=self.root))

    def write_compilation_database(self, context: FactsToolContext, options: list[str]) -> None:
        arguments = [str(context.compiler), *options, "-c", str(self.source)]
        (self.root / "compile_commands.json").write_text(json.dumps([{
            "directory": str(self.root), "file": str(self.source), "arguments": arguments
        }]))

    def run_database_import(self, context: FactsToolContext, conf: Path) -> None:
        self.conf = conf
        self._record(context, subprocess.run(
            [str(context.facts_tool), "import", "--conf", str(conf),
             "--compilation-database", str(self.root)],
            capture_output=True, text=True, check=False))

    def extract(self, context: FactsToolContext) -> None:
        result = subprocess.run([str(context.facts_tool), "extract", "--output",
                                 str(self.root / "facts.sqlite"), "--conf", str(self.conf),
                                 str(self.source)], capture_output=True, text=True, check=False)
        self._record(context, result)

    def stored_options(self) -> list[str]:
        with sqlite3.connect(self.conf) as connection:
            (encoded,) = connection.execute(
                "SELECT compile_options FROM file WHERE name=?", (self.source.name,)
            ).fetchone()
        return json.loads(encoded)

    def _record(self, context: FactsToolContext, result: subprocess.CompletedProcess[str]) -> None:
        context.last_returncode = result.returncode
        context.last_output = result.stdout + result.stderr
        context.files_database = self.conf
