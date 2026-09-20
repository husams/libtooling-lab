"""Readiness, instance exclusion, persistence and graceful process shutdown."""
import socket
import subprocess

from pytest_bdd import then, when
from support.rest_server import persisted_port


@then("the server configuration stores its automatically allocated listening port")
def saved_port(rest_server):
    assert 0 < rest_server.api.port <= 65535
    assert persisted_port(rest_server.config) == rest_server.api.port
    rest_server.original_port = rest_server.api.port


@when("I restart the server using only its saved configuration")
def restart(rest_server):
    rest_server.shutdown()
    rest_server.output.close()
    rest_server.start(reuse=True)


@then("the restarted server reuses the saved port")
def same_port(rest_server):
    assert rest_server.api.port == rest_server.original_port
    assert persisted_port(rest_server.config) == rest_server.original_port


@when("I restart while another listener occupies the saved port")
def occupied_restart(rest_server):
    rest_server.shutdown()
    rest_server.output.close()
    with socket.socket() as occupied:
        occupied.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        occupied.bind(("127.0.0.1", rest_server.original_port))
        occupied.listen()
        rest_server.start(reuse=True)


@then("the server saves a different available port and accepts requests")
def replacement_port(rest_server):
    assert rest_server.api.port != rest_server.original_port
    assert persisted_port(rest_server.config) == rest_server.api.port
    assert rest_server.api.request("GET", "/health")[0] == 200


@then("the daemon launcher has succeeded and its child is ready with a PID")
def daemon_ready(rest_server):
    assert rest_server.process.wait(timeout=10) == 0
    assert int(rest_server.pid_file.read_text().strip()) > 1
    assert rest_server.api.request("GET", "/health")[0] == 200


@when("another server tries to use the same server configuration")
def duplicate(rest_server):
    rest_server.duplicate = subprocess.run(
        [str(rest_server.executable), "serve", "--server-config", str(rest_server.config)],
        cwd=rest_server.root, env=rest_server.environment, capture_output=True,
        text=True, timeout=10, check=False)


@then("the duplicate instance fails while the original continues serving requests")
def excluded(rest_server):
    assert rest_server.duplicate.returncode != 0
    assert rest_server.api.request("GET", "/health")[0] == 200


@when("I request graceful server shutdown through REST")
def shutdown(rest_server):
    rest_server.shutdown()


@then("the server releases its PID lock and listening socket")
def stopped(rest_server):
    assert not rest_server.pid_file.read_text().strip()
    with socket.socket() as probe:
        assert probe.connect_ex(("127.0.0.1", rest_server.api.port)) != 0
