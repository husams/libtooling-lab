from __future__ import annotations
import json
import os
import subprocess
from pathlib import Path

class Defaults:
    def __init__(self, tool, root):
        self.tool, self.root = tool, root.resolve()
        self.cwd = self.root / "project.v2"
        self.cwd.mkdir()
        (self.cwd / ".git").mkdir()
        self.env = {k: v for k, v in os.environ.items()
                    if not k.startswith(("FACTS_TOOL_", "XDG_")) and k != "HOME"}
        self.env["HOME"] = str(self.root / "home")
        self.files = {
            "cli": self.root / "cli.yaml", "env": self.root / "env.yaml",
            "project": self.cwd / ".facts-tool.yaml",
            "xdg": self.root / "xdg/facts-tool/config.yaml",
            "home": self.root / "home/.config/facts-tool/config.yaml"}
        self.args = []
        self.last = None

    def write(self, tier="project", **values):
        path = self.files[tier]
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(values), encoding="utf-8")
        return path

    def run(self, *args, cwd=None, env=None):
        self.last = subprocess.run([str(self.tool), *map(str, args)],
            cwd=cwd or self.cwd, env=self.env if env is None else env,
            text=True, capture_output=True)
        return self.last

    def show(self):
        return self.run("config", "show", *self.args)

    def value(self, key):
        line = next(x for x in self.last.stdout.splitlines() if x.startswith(key + ": "))
        value = line.split(": ", 1)[1]
        return json.loads(value) if value.startswith('"') else value

    def expected(self, tier):
        if tier in ("direct", "db-env"):
            return self.root / (tier + ".db")
        if tier == "builtin":
            return (self.root / "home/.local/share/facts-tool" /
                    self.cwd.parent.relative_to("/") / (self.cwd.name + ".db"))
        return self.root / ("store-" + tier) / (self.cwd.name + ".db")

    def tiers(self, names):
        for tier in names.split(","):
            if tier == "none" or tier == "builtin":
                continue
            if tier == "direct":
                self.args += ["--conf", str(self.expected(tier))]
            elif tier == "db-env":
                self.env["FACTS_TOOL_CONF"] = str(self.expected(tier))
            else:
                self.write(tier, conf_root=str(self.root / ("store-" + tier)),
                    conf_template="{filename}.db", extra_args=["-DTIER=" + tier])
                if tier == "cli": self.args += ["--config", str(self.files[tier])]
                if tier == "env": self.env["FACTS_TOOL_CONFIG"] = str(self.files[tier])
                if tier == "xdg": self.env["XDG_CONFIG_HOME"] = str(self.root / "xdg")

    def snapshot(self):
        return {str(p.relative_to(self.root)): p.read_bytes()
                for p in self.root.rglob("*") if p.is_file()}
