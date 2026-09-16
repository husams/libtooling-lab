"""Isolated native CLI fixture for persisted Clang AST acceptance tests."""
from __future__ import annotations

import json
import os
import subprocess
from dataclasses import dataclass, field
from pathlib import Path

HEADER = """#pragma once
struct HeaderBefore { int value; };
inline int cache_adjust(int value) { return value + 1; }
"""
SOURCE = """#include "cache.hpp"
#ifndef CACHE_VALUE
#define CACHE_VALUE 7
#endif
#ifdef CACHE_MODE
int cache_mode_enabled() { return 1; }
#endif
template<class T> T cache_identity(T value) { return value; }
int cache_leaf(int input) { return cache_adjust(cache_identity(input)); }
int cache_root() {
  int seed = CACHE_VALUE;
  int result = cache_leaf(seed);
  return result;
}
"""


@dataclass
class AstCacheProject:
    tool: Path
    compiler: Path
    root: Path
    environment: dict[str, str]
    cache: Path
    selected_config: Path | None = None
    last: subprocess.CompletedProcess[str] | None = None
    baseline: dict = field(default_factory=dict)
    cache_before: dict = field(default_factory=dict)

    @classmethod
    def create(cls, context, root):
        root = root.resolve()
        root.mkdir(parents=True, exist_ok=True)
        (root / ".git").mkdir()
        (root / "cache.hpp").write_text(HEADER, encoding="utf-8")
        (root / "cache.cpp").write_text(SOURCE, encoding="utf-8")
        environment = {key: value for key, value in os.environ.items()
                       if not key.startswith(("FACTS_TOOL_", "XDG_"))}
        environment["XDG_CONFIG_HOME"] = str(root / "user-config")
        environment["XDG_DATA_HOME"] = str(root / "user-data")
        project = cls(context.facts_tool, context.compiler, root, environment,
                      root / ".facts-tool" / "ast-cache")
        project.write_commands()
        project.run("import")
        project.succeed()
        return project

    @property
    def source(self):
        return self.root / "cache.cpp"

    @property
    def header(self):
        return self.root / "cache.hpp"

    @property
    def conf(self):
        return self.root / "project.sqlite"

    @property
    def facts(self):
        return self.root / "facts.sqlite"

    def write_commands(self, *extra):
        output = str(self.root / "cache.o")
        commands = [{"directory": str(self.root), "file": str(self.source),
                     "output": output,
                     "arguments": [str(self.compiler), "-std=c++23", *extra,
                                   "-c", str(self.source), "-o", output]}]
        (self.root / "compile_commands.json").write_text(json.dumps(commands),
                                                       encoding="utf-8")

    def configure(self, *, tier="project", **settings):
        paths = {"project": self.root / ".facts-tool.yaml",
                 "user": self.root / "user-config/facts-tool/config.yaml",
                 "explicit": self.root / "explicit.yaml"}
        path = paths[tier]
        path.parent.mkdir(parents=True, exist_ok=True)
        # JSON is a YAML subset and keeps absolute paths safely quoted.
        path.write_text(json.dumps({"facts_template": str(self.facts), **settings}),
                        encoding="utf-8")
        if tier == "explicit":
            self.selected_config = path
        if "ast_cache_dir" in settings:
            self.cache = Path(settings["ast_cache_dir"])
            if not self.cache.is_absolute():
                self.cache = self.root / self.cache

    def command(self, family, *extra):
        families = {"extract": ["extract", "--force", "--output", str(self.facts)],
                    "import": ["import", "--facts", str(self.facts), "-p", str(self.root)],
                    "match": ["match", "--facts", str(self.facts), "--matcher",
                              'functionDecl(hasName("cache_root")).bind("symbol")'],
                    "dependency": ["analyse", "dependency", "--output", str(self.facts)],
                    "variable-flow": ["analyse", "variable-flow", "--function", "cache_root",
                                      "--variable", "seed", "--output", str(self.root / "flow.sqlite")],
                    "show": ["config", "show"]}
        options = ["--conf", str(self.conf)]
        if family != "show":
            options += ["-v", "1"]
        if self.selected_config is not None:
            options += ["--config", str(self.selected_config)]
        sources = [] if family == "show" else [str(self.source)]
        return [str(self.tool), *families[family], *options, *map(str, extra), *sources]

    def run(self, family, *extra, command=None, cwd=None):
        self.last = subprocess.run(command or self.command(family, *extra), cwd=cwd or self.root,
                                   env=self.environment, text=True, capture_output=True,
                                   timeout=60, check=False)
        return self.last

    def succeed(self):
        assert self.last is not None
        assert self.last.returncode == 0, self.last.stdout + self.last.stderr

    def ast_files(self):
        return tuple(self.cache.rglob("*.ast")) if self.cache.is_dir() else ()

    def snapshot_cache(self):
        return {str(path.relative_to(self.cache)): (path.read_bytes(), path.stat().st_mtime_ns)
                for path in self.cache.rglob("*") if path.is_file()}
