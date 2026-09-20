"""Unsafe or unusable log destinations fail before advertising readiness."""
import os
import socket
import subprocess

import pytest
from logging_support import events
from server import isolated_environment
from support import eventually


def launch(executable, root, *options):
    return subprocess.run([str(executable), "serve", "--server-config",
                           str(root / "server.yaml"), "--port", "0", *map(str, options)],
                          cwd=root, env=isolated_environment(root),
                          capture_output=True, text=True, timeout=10, check=False)


@pytest.mark.parametrize("destination", ["directory", "file-parent", "fifo", "symlink"])
@pytest.mark.parametrize("daemon", [False, True])
def test_unusable_log_destination_never_reports_ready(executable, tmp_path,
                                                       destination, daemon):
    log = tmp_path / "chosen.log"
    if destination == "directory":
        log.mkdir()
    elif destination == "file-parent":
        (tmp_path / "parent").write_text("not a directory")
        log = tmp_path / "parent" / "chosen.log"
    elif destination == "fifo":
        os.mkfifo(log)
    else:
        target = tmp_path / "target.log"
        target.write_text("preserve existing content\n")
        log.symlink_to(target)
    result = launch(executable, tmp_path, "--log-file", log,
                    *(["--daemon"] if daemon else []))
    assert result.returncode != 0, result
    assert "log" in result.stderr.lower(), result.stderr
    assert not (tmp_path / "server.yaml").exists()
    if destination == "symlink":
        assert target.read_text() == "preserve existing content\n"


def test_daemon_error_points_to_custom_log(executable, tmp_path):
    log = tmp_path / "custom.log"
    with socket.socket() as occupied:
        occupied.bind(("127.0.0.1", 0))
        occupied.listen()
        result = subprocess.run([str(executable), "serve", "--server-config",
            str(tmp_path / "server.yaml"), "--daemon", "--log-file", str(log),
            "--port", str(occupied.getsockname()[1])], cwd=tmp_path,
            env=isolated_environment(tmp_path), capture_output=True, text=True,
            timeout=10, check=False)
    assert result.returncode != 0
    assert str(log) in result.stderr
    assert "server.failed" in events(log)
    assert "server.ready" not in events(log)


def test_duplicate_instance_cannot_create_or_truncate_logs(server_factory, executable,
                                                         tmp_path):
    active = tmp_path / "active.log"
    server = server_factory("--log-file", active)
    eventually(lambda: "server.ready" in events(active))
    original = active.read_bytes()
    rejected = tmp_path / "rejected.log"
    result = subprocess.run([str(executable), "serve", "--server-config",
        str(server.config), "--log-file", str(rejected)], cwd=server.root,
        env=server.env, capture_output=True, text=True, timeout=10, check=False)
    assert result.returncode != 0
    assert active.read_bytes().startswith(original)
    assert not rejected.exists()
    assert server.api.request("GET", "/health")[0] == 200
