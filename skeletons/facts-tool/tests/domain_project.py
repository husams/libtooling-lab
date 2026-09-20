"""Real cross-repository C++ fixtures shared by REST integration and BDD tests."""
import json
import sqlite3
import subprocess


class DomainProject:
    def __init__(self, executable, root, compiler, environment):
        self.executable, self.root = executable, root
        self.compiler, self.environment = compiler, environment
        root.mkdir(parents=True, exist_ok=True)
        self.database, self.defaults = root / "project.db", root / "defaults.yaml"
        template = str(root / "facts/{relative_path}/{filename}.db")
        self.defaults.write_text(f"facts_template: {json.dumps(template)}\n")
        self.roots, self.sources, self.facts = {}, {}, {}

    def cli(self, *arguments):
        result = subprocess.run(
            [str(self.executable), *map(str, arguments), "--conf", str(self.database),
             "--config", str(self.defaults)], cwd=self.root, env=self.environment,
            capture_output=True, text=True, timeout=30, check=False)
        assert result.returncode == 0, result.stdout + result.stderr
        return result

    def add(self, name, extract=True, label=True):
        root = self.root / f"checkout-{name}"
        (root / "src").mkdir(parents=True)
        subprocess.run(["git", "init", "-q", str(root)], check=True)
        source = root / "src/main.cpp"
        (source.parent / "common.hpp").write_text(
            f"#pragma once\nnamespace {name} {{ inline int helper() {{ return 7; }} }}\n")
        self.write_source(source, name, "answer")
        self.commands(root, source)
        labels = ["--label", f"primary-{name}"] if label else []
        self.cli("repo", "add", name, root, *labels)
        self.cli("component", "add", "--name", f"{name}-core", "--path", source.parent,
                 "--kind", "repo", "--repo", name, "--no-git")
        clone = self.rows("SELECT active_clone_id FROM repository WHERE name=?", (name,))[0][0]
        self.cli("import", "-p", root, "--existing-clone", clone)
        facts = self.root / f"{name}-facts.db"
        if extract:
            self.cli("extract", "-o", facts, source)
        self.roots[name], self.sources[name], self.facts[name] = root, source, facts
        return source

    def commands(self, root, source):
        (root / "compile_commands.json").write_text(json.dumps([
            {"directory": str(root), "file": str(source),
             "arguments": [str(self.compiler), "-std=c++17", "-c", str(source)]}]))

    @staticmethod
    def write_source(source, namespace, symbol):
        source.write_text('#include "common.hpp"\n'
                          f"namespace {namespace} {{ class Widget {{}}; "
                          f"int {symbol}() {{ return helper(); }} }}\n"
                          "namespace shared { int same() { return 1; } }\n")

    def inactive_clone(self, name="alpha"):
        root = self.root / f"secondary-{name}"
        (root / "src").mkdir(parents=True)
        source = root / "src/main.cpp"
        (source.parent / "common.hpp").write_bytes(
            (self.sources[name].parent / "common.hpp").read_bytes())
        self.write_source(source, name, "inactive")
        self.commands(root, source)
        subprocess.run(["git", "init", "-q", str(root)], check=True)
        self.cli("repo", "add-clone", name, root, "--label", "secondary")
        return source

    def rows(self, sql, parameters=()):
        with sqlite3.connect(self.database.as_uri() + "?mode=ro", uri=True) as database:
            return database.execute(sql, parameters).fetchall()

    def options(self):
        return ["--conf", self.database, "--config", self.defaults, "--no-watch"]
