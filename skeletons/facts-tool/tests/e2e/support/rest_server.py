"""Process ownership and isolated persistence for native REST BDD scenarios."""
import os
import re
import signal
import subprocess
from pathlib import Path

from support.rest_http import RestApi, eventually


def process_stopped(pid):
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return True
    status = Path(f"/proc/{pid}/stat")
    return status.exists() and status.read_text().split(") ", 1)[1].startswith("Z")


def persisted_port(path):
    if not path.exists():
        return None
    match = re.search(r"^port:\s*(\d+)\s*$", path.read_text(), re.MULTILINE)
    return int(match.group(1)) if match and int(match.group(1)) else None


class RestServer:
    def __init__(self, executable, root):
        self.executable, self.root = executable, root
        root.mkdir(parents=True, exist_ok=True)
        self.config = root / "server.yaml"
        self.pid_file = root / "server.yaml.pid"
        self.environment = {k: v for k, v in os.environ.items()
                            if not k.startswith("FACTS_TOOL_")}
        self.environment.update(FACTS_TOOL_API_TOKEN="bdd-secret",
                                XDG_CONFIG_HOME=str(root / "config"),
                                XDG_DATA_HOME=str(root / "data"))
        self.process, self.output, self.api = None, None, None

    def start(self, options=(), reuse=False):
        command = [str(self.executable), "serve", "--server-config", str(self.config)]
        command += [] if reuse else ["--port", "0"]
        self.log = self.root / "server.log"
        self.output = self.log.open("w")
        self.process = subprocess.Popen([*command, *map(str, options)], cwd=self.root,
                                        env=self.environment, stdout=self.output,
                                        stderr=subprocess.STDOUT)
        def ready():
            assert self.process.poll() in {None, 0}, self.log.read_text()
            port = persisted_port(self.config)
            if port:
                api = RestApi(port)
                if api.request("GET", "/health")[0] == 200:
                    return api
            return None
        if "--daemon" in options:
            assert self.process.wait(timeout=10) == 0, self.log.read_text()
            self.api = ready()
            assert self.api, "daemon launcher returned before the listener was ready"
        else:
            self.api = eventually(ready, timeout=15)
        return self

    def shutdown(self):
        status, _ = self.api.request("POST", "/v1/shutdown", {})
        assert status in {200, 202}
        assert self.process.wait(timeout=10) == 0
        eventually(lambda: self.pid_file.exists() and not self.pid_file.read_text().strip())

    def close(self):
        if self.api:
            try:
                self.api.request("POST", "/v1/shutdown", {})
            except (OSError, ValueError):
                pass
        if self.pid_file.exists():
            try:
                eventually(lambda: not self.pid_file.read_text().strip(), timeout=3)
            except AssertionError:
                try:
                    pid = int(self.pid_file.read_text().strip())
                    os.kill(pid, signal.SIGTERM)
                    try:
                        eventually(lambda: process_stopped(pid), timeout=3)
                    except AssertionError:
                        os.kill(pid, signal.SIGKILL)
                        eventually(lambda: process_stopped(pid), timeout=3)
                except (ProcessLookupError, ValueError):
                    pass
        if self.process:
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=3)
        if self.output:
            self.output.close()
