from __future__ import annotations

import json
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
    def create(cls, context: FactsToolContext, local_header: bool) -> ForcedIncludeFixture:
        context.output_root.mkdir(parents=True, exist_ok=True)
        root = Path(tempfile.mkdtemp(prefix="b028-", dir=context.output_root)).resolve()
        source = root / "optional.cpp"
        source.write_text("""// Intentionally omit <optional>; supply it with -include optional.
std::optional<int> maybe_value(bool enabled) {
  return enabled ? std::optional<int>{42} : std::nullopt;
}

int main() {
  return maybe_value(true).value_or(0) == 42 ? 0 : 1;
}
""")
        header = None
        if local_header:
            header = root / "include" / "forced.hpp"
            header.parent.mkdir()
            header.write_text("#define B028_FORCED_VALUE 42\n")
            source = root / "paths.cpp"
            source.write_text("int forced_value() { return B028_FORCED_VALUE; }\n")
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
