import json
import shlex
import sqlite3
from support.retention_sources import write_sources


class Retention:
    def __init__(self, defaults, representation, driver):
        self.d = defaults
        self.db = defaults.root / "project.db"
        self.cli, self.yaml, self.version = [], False, "YAML"
        self.runtime = False
        self.results = []
        write_sources(defaults)
        self.sources = [defaults.cwd / "unit_a.cpp", defaults.cwd / "sub/unit_b.cpp"]
        self.directories = [defaults.cwd, defaults.cwd / "sub"]
        self.base = [
            [driver, "-std=c++23", "-DJSON_A=1", "-I", "include/json a",
             "-I", "../outside/json_inc", "-DJSON_SPACE=json words", str(self.sources[0])],
            [driver, "-std=c++20", "-DJSON_B=1", "-include", "base/json_b.hpp",
             str(self.sources[1])]]
        self.representation = representation
        self.write_database()

    def write_database(self):
        entries = []
        for source, directory, args in zip(self.sources, self.directories, self.base):
            entry = {"directory": str(directory), "file": str(source)}
            entry[self.representation] = args if self.representation == "arguments" else shlex.join(args)
            entries.append(entry)
        (self.d.cwd / "compile_commands.json").write_text(json.dumps(entries, indent=2))

    def configure(self, mode="enabled", version="YAML", conflict=False):
        self.yaml, self.version = mode == "enabled", version
        args = [f"-D{version}_ONLY=1", "-include", str(self.d.cwd / (version + " header.hpp")),
                "-DYAML_SPACE=yaml words"] if self.yaml else []
        if conflict:
            args += ["-DVALUE=2"]
            for base in self.base:
                base.insert(1, "-DVALUE=1")
            self.write_database()
        self.d.write(**({} if mode == "absent" else {"extra_args": args}))

    def run(self, family, cli=False, runtime=False):
        self.runtime = runtime
        if family == "import":
            args = ["import", "--component", "fixture=.", "-p", str(self.d.cwd)]
        else:
            args = ["extract"] if family == "extract" else ["analyse", "dependency"]
            args += ["-o", str(self.d.root / (family + ".db")), *map(str, self.sources)]
        extras = ["--extra-arg=-DCLI_ONLY=1 '-DCLI_SPACE=two words'"] if cli else []
        if runtime:
            extras += ["--extra-arg=-DRUNTIME_ONLY=1 '-DRUNTIME_SPACE=runtime words.hpp'"]
        result = self.d.run(*args, "--conf", str(self.db), *extras)
        self.results.append((family, result))
        assert result.returncode == 0, result.stdout + result.stderr
        return result

    def stored(self):
        result = self.d.run("component", "compile-commands", "fixture", "--conf", str(self.db))
        assert result.returncode == 0, result.stderr
        return sorted(json.loads(result.stdout), key=lambda entry: entry["file"])

    def snapshot(self):
        with sqlite3.connect(self.db) as db:
            return db.execute("SELECT driver,compile_options,working_directory FROM file "
                              "WHERE driver IS NOT NULL ORDER BY id").fetchall()

    def execute(self, family, repeats=1, cli=False):
        self.cli = ["-DCLI_ONLY=1", "-DCLI_SPACE=two words"] if cli else []
        self.run("import", cli)
        self.original = self.snapshot()
        from support.retention_assertions import effects, commands
        for run in range(repeats):
            if family != "import" or run:
                self.run(family, cli)
            effects(self, family)
            commands(self)
