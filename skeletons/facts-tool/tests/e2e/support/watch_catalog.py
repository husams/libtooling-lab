"""Real repository catalogs and processes for database-driven watch scenarios."""
import json
import sqlite3
import subprocess

from support.database import file_snapshot
from support.rest_http import eventually
from support.rest_project import watch_status, write_commands


class WatchCatalog:
    def __init__(self, server, compiler):
        self.server, self.compiler = server, compiler
        self.database = server.root / "project.db"
        self.defaults = server.root / "defaults.yaml"
        self.defaults.write_text(f"facts_template: {server.root / 'facts.db'}\n")
        self.roots, self.sources, self.policy = {}, {}, {}

    def cli(self, *arguments):
        result = subprocess.run(
            [str(self.server.executable), *map(str, arguments), "--conf",
             str(self.database), "--config", str(self.defaults)],
            cwd=self.server.root, env=self.server.environment,
            capture_output=True, text=True, timeout=30, check=False)
        assert result.returncode == 0, result.stdout + result.stderr
        return result

    def create(self, name, paths=("main.cpp",), label=None, git=True):
        root = self.server.root / f"checkout-{name}"
        root.mkdir()
        if git:
            subprocess.run(["git", "init", "-q", str(root)], check=True)
        self.roots[name] = root
        self.sources[name] = []
        for index, relative in enumerate(paths):
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(f"int {name}_original_{index}() {{ return {index}; }}\n")
            self.sources[name].append(path)
        write_commands(root, self.compiler, self.sources[name])
        self.cli("repo", "add", name, root, "--label", label or f"primary-{name}")
        clone = self.rows("SELECT active_clone_id FROM repository WHERE name=?", [name])[0][0]
        self.cli("import", "-p", root, "--existing-clone", clone)
        self.cli("extract", *self.sources[name])
        return root

    def start(self, names=None):
        if self.policy:
            self.server.config.write_text("watch:\n" + "".join(
                f"  {key}: {json.dumps(value)}\n" for key, value in self.policy.items()))
        self.server.start(["--conf", self.database, "--config", self.defaults,
                           "--debounce-ms", "40"])
        assert "watch_directories" not in self.server.config.read_text()
        self.expect_roots(names if names is not None else self.roots)

    def expect_roots(self, names):
        expected = {str(self.roots[name]) for name in names}
        def ready():
            state = watch_status(self.server)
            assert not state["last_error"], state
            assert state["source"] == "project_database", state
            assert state["project_database"] == str(self.database), state
            return state if set(state["directories"]) == expected and state["ready"] else None
        return eventually(ready)

    def rows(self, sql, parameters=()):
        with sqlite3.connect(self.database.as_uri() + "?mode=ro", uri=True) as db:
            return db.execute(sql, parameters).fetchall()

    def identities(self):
        return self.rows("SELECT r.name,c.path,c.label,r.active_clone_id "
                         "FROM repository r JOIN clone c ON c.repository_id=r.id "
                         "ORDER BY r.id,c.id")

    def snapshot(self, paths):
        selected = {str(path) for path in paths}
        identifiers = [identifier for identifier, path in file_snapshot(self.database)
                       if path in selected]
        assert len(identifiers) == len(selected)
        return self.rows("SELECT id,name,indexed,indexed_at,md5,compile_options "
                         f"FROM file WHERE id IN ({','.join('?' for _ in identifiers)}) "
                         "ORDER BY id", identifiers)

    def symbols(self):
        return self.server.api.run([], "symbol/list")["stdout"]
