"""Launch real facts-tool processes with isolated configuration and cleanup."""
import os
import re
import subprocess

from support import Api, eventually


def isolated_environment(root):
    env = {key: value for key, value in os.environ.items()
           if key not in {"FACTS_TOOL_CONF", "FACTS_TOOL_CONFIG", "FACTS_TOOL_API_TOKEN"}}
    env.update(XDG_CONFIG_HOME=str(root / "config"), XDG_DATA_HOME=str(root / "data"))
    return env


def persisted_port(path):
    if not path.exists():
        return None
    match = re.search(r"^port:\s*(\d+)\s*$", path.read_text(), re.MULTILINE)
    return int(match.group(1)) if match and int(match.group(1)) else None


class Server:
    def __init__(self, executable, root, options=(), token=None, cwd=None):
        self.root = root
        root.mkdir(parents=True, exist_ok=True)
        self.config = root / "server.yaml"
        self.env = isolated_environment(root)
        self.command = [str(executable), "serve", "--server-config", str(self.config),
                        "--port", "0", *map(str, options)]
        if token:
            self.env["FACTS_TOOL_API_TOKEN"] = token
        self.log = root / "process.log"
        self.output = self.log.open("w")
        self.process = subprocess.Popen(self.command, cwd=cwd or root, env=self.env,
                                        stdout=self.output, stderr=subprocess.STDOUT)
        try:
            def ready_port():
                if self.process.poll() not in {None, 0}:
                    raise RuntimeError("server exited before readiness")
                return persisted_port(self.config)
            port = eventually(ready_port)
            self.api = Api(port, token)
            eventually(lambda: self.api.request("GET", "/health")[0] == 200)
        except BaseException:
            self.close()
            raise AssertionError(self.log.read_text())

    def close(self):
        if hasattr(self, "api"):
            try:
                self.api.request("POST", "/v1/shutdown", {})
            except (OSError, ValueError):
                pass
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=3)
        self.output.close()
