"""Daemon readiness, startup exclusion and configuration reuse."""
import socket
import subprocess

from server import persisted_port
from support import Api, eventually


def test_duplicate_server_config_is_rejected(server, executable):
    result = subprocess.run([str(executable), "serve", "--server-config", str(server.config)],
                            cwd=server.root, env=server.env, capture_output=True,
                            text=True, timeout=10)
    assert result.returncode != 0
    assert server.api.request("GET", "/health")[0] == 200


def test_graceful_shutdown(server):
    status, _ = server.api.request("POST", "/v1/shutdown", {})
    assert status in {200, 202}
    assert server.process.wait(timeout=10) == 0


def test_daemon_returns_only_after_ready(server_factory):
    server = server_factory("--daemon")
    assert server.process.wait(timeout=10) == 0
    pid_file = server.config.with_name(server.config.name + ".pid")
    assert int(pid_file.read_text().strip()) > 1
    assert server.api.request("GET", "/health")[0] == 200
    server.api.request("POST", "/v1/shutdown", {})
    eventually(lambda: pid_file.exists() and not pid_file.read_text().strip())


def test_saved_port_reused_on_restart(server, executable):
    port = server.api.port
    server.close()
    output = (server.root / "restart.log").open("w")
    process = subprocess.Popen([str(executable), "serve", "--server-config", str(server.config)],
                               cwd=server.root, env=server.env, stdout=output, stderr=output)
    try:
        eventually(lambda: server.api.request("GET", "/health")[0] == 200)
        assert server.api.port == port
        server.api.request("POST", "/v1/shutdown", {})
        assert process.wait(timeout=10) == 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=3)
        output.close()


def test_busy_saved_port_is_reallocated_and_saved(server, executable):
    previous = server.api.port
    server.close()
    with socket.socket() as occupied, (server.root / "restart.log").open("w") as output:
        occupied.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        occupied.bind(("127.0.0.1", previous))
        occupied.listen()
        process = subprocess.Popen(
            [str(executable), "serve", "--server-config", str(server.config)],
            cwd=server.root, env=server.env, stdout=output, stderr=output)
        try:
            def changed_port():
                port = persisted_port(server.config)
                return port if port and port != previous else None
            api = Api(eventually(changed_port))
            eventually(lambda: api.request("GET", "/health")[0] == 200)
            api.request("POST", "/v1/shutdown", {})
            assert process.wait(timeout=10) == 0
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=3)
