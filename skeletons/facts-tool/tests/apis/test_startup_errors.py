"""Startup errors must reach the caller without leaving a false ready record."""
import socket
import subprocess

import pytest

from server import isolated_environment


def serve(executable, root, *options):
    config = root / "server.yaml"
    return subprocess.run([str(executable), "serve", "--server-config", str(config),
                           *map(str, options)], cwd=root, env=isolated_environment(root),
                          capture_output=True, text=True, timeout=10)


def test_explicit_occupied_port_fails(executable, tmp_path):
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        result = serve(executable, tmp_path, "--port", listener.getsockname()[1])
    assert result.returncode != 0
    assert not (tmp_path / "server.yaml").exists()


def test_daemon_watch_error_reaches_parent(executable, tmp_path):
    result = serve(executable, tmp_path, "--daemon", "--watch", tmp_path / "missing")
    assert result.returncode != 0
    assert not (tmp_path / "server.yaml").exists()


def test_non_loopback_requires_token(executable, tmp_path):
    result = serve(executable, tmp_path, "--host", "0.0.0.0")
    assert result.returncode != 0
    assert "token" in result.stderr.lower()


def test_invalid_saved_configuration(executable, tmp_path):
    (tmp_path / "server.yaml").write_text("port: 99999\n")
    result = serve(executable, tmp_path)
    assert result.returncode != 0
    assert "port" in result.stderr.lower()


@pytest.mark.parametrize("defaults", ['["--conf"]', '["--conf=x"]',
                                     '["--unknown", "value"]'])
def test_invalid_default_pairs_rejected(executable, tmp_path, defaults):
    (tmp_path / "server.yaml").write_text(f"defaults: {defaults}\n")
    result = serve(executable, tmp_path)
    assert result.returncode != 0
