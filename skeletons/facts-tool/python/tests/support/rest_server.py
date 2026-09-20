import os
import re
import subprocess
import time
from contextlib import contextmanager
from pathlib import Path

from support.native_cli_helpers import tool


@contextmanager
def native_server(root: Path):
    """Launch the real native API with isolated project/server configuration."""
    from facts_tool.rest import Client

    root.mkdir(parents=True, exist_ok=True)
    config = root / "server.yaml"
    defaults = root / "defaults.yaml"
    defaults.write_text("{}\n")
    environment = {
        name: value
        for name, value in os.environ.items()
        if not name.startswith("FACTS_TOOL_")
    }
    with (root / "server.log").open("w+") as log:
        process = subprocess.Popen(
            [
                str(tool()),
                "serve",
                "--server-config",
                str(config),
                "--port",
                "0",
                "--config",
                str(defaults),
                "--token",
                "integration-token",
            ],
            cwd=root,
            env=environment,
            stdout=log,
            stderr=log,
        )
        try:
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    log.seek(0)
                    raise AssertionError(log.read())
                if config.exists():
                    port = re.search(r"^port: (\d+)$", config.read_text(), re.M)
                    if port:
                        url = f"http://127.0.0.1:{port[1]}"
                        with Client(url, token="integration-token") as client:
                            assert client.health()["status"] == "ok"
                        yield url
                        return
                time.sleep(0.02)
            raise AssertionError("native REST server did not become ready")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
