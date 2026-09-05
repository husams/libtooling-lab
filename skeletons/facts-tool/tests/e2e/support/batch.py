import json
import os
import shutil
import sqlite3
import subprocess
import sys
from pathlib import Path

WRAPPER = Path(__file__).resolve().parents[3] / "scripts" / "facts-tool-batch"


class BatchProject:
    def __init__(self, context, root):
        self.root = root / "batch project"
        shutil.copytree(context.fixture_root.parent / "batch", self.root)
        self.sources = sorted(self.root.rglob("*.cpp"))
        self.configuration = self.root / "project.db"
        self.output = self.root / "facts"
        self.env = dict(os.environ)
        for name in ("FACTS_TOOL_CONF", "FACTS_TOOL_CONFIG", "XDG_DATA_HOME"):
            self.env.pop(name, None)
        self.env["XDG_CONFIG_HOME"] = str(root / "no-user-defaults")
        self.yaml = self.root / ".facts-tool.yaml"
        self.yaml.write_text("extra_args: []\n")
        self.env["PATH"] = (
            str(context.facts_tool.parent) + os.pathsep + os.environ["PATH"]
        )
        self.compdb = self.root / "compile_commands.json"
        self.compdb.write_text(
            json.dumps(
                [
                    {
                        "directory": str(self.root),
                        "file": str(source),
                        "arguments": [
                            str(context.compiler),
                            "-std=c++17",
                            "-c",
                            str(source),
                        ],
                    }
                    for source in self.sources
                ]
            )
        )
        result = self.run(
            [
                str(context.facts_tool),
                "import",
                "-c",
                str(self.configuration),
                "--config",
                str(self.yaml),
                "-p",
                str(self.root),
            ]
        )
        assert result.returncode == 0, result.stdout

    def run(self, command):
        return subprocess.run(
            command,
            cwd=self.root,
            env=self.env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=60,
        )

    def batch(self, mode, jobs, inputs):
        command = [
            sys.executable,
            str(WRAPPER),
            mode,
            "-j",
            str(jobs),
            "-c",
            str(self.configuration),
            "--config",
            str(self.yaml),
            "-o",
            str(self.output),
        ]
        if inputs == "compdb":
            command += ["-p", str(self.root)]
        elif inputs == "list":
            listing = self.root / "sources.txt"
            listing.write_text("\n".join(map(str, self.sources)) + "\n")
            command += ["--files-from", str(listing), str(self.sources[0])]
        else:
            command += list(map(str, self.sources))
        self.result = self.run(command)

    def verify(self, mode):
        assert self.result.returncode == 0, self.result.stdout
        databases = sorted(self.output.glob("*.db"))
        assert len(databases) == len(self.sources)
        names = set()
        with sqlite3.connect(self.configuration) as db:
            header = db.execute(
                "SELECT id FROM file WHERE name='shared.hpp'"
            ).fetchone()[0]
        for path in databases:
            with sqlite3.connect(path) as db:
                assert db.execute("PRAGMA integrity_check").fetchone() == ("ok",)
                if mode == "extract":
                    names.update(
                        row[0]
                        for row in db.execute("SELECT qualified_name FROM symbol")
                    )
                else:
                    assert (header,) in db.execute(
                        "SELECT dst_file_id FROM include_dependency"
                    ).fetchall()
        if mode == "extract":
            assert {"batch_left", "batch_right", "batch_words", "batch_four"} <= names
