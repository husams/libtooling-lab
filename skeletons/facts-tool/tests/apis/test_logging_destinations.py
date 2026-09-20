"""Log creation must preserve server data and enforce private file permissions."""
import os
import subprocess
from pathlib import Path

import pytest
from server import isolated_environment


def test_nested_log_directory_is_created_privately(server_factory, tmp_path):
    path = tmp_path / "private" / "nested" / "events.jsonl"
    server = server_factory("--log-file", path)
    assert path.exists()
    assert path.stat().st_mode & 0o777 == 0o600
    assert path.parent.stat().st_mode & 0o777 == 0o700
    assert path.parent.parent.stat().st_mode & 0o777 == 0o700
    assert server.api.request("GET", "/health")[0] == 200


@pytest.mark.parametrize("kind", ["configuration", "hardlink", "pid", "project"])
def test_log_cannot_overwrite_configuration_or_database(executable, tmp_path, kind):
    config = tmp_path / "server.yaml"
    original = "host: 127.0.0.1\n"
    config.write_text(original)
    target = config
    options = []
    if kind == "hardlink":
        target = tmp_path / "aliased.log"
        os.link(config, target)
    elif kind == "pid":
        target = Path(str(config) + ".pid")
    elif kind == "project":
        target = tmp_path / "project.db"
        target.write_text("preserve project database bytes")
        options = ["--conf", str(target)]
    result = subprocess.run([str(executable), "serve", "--server-config", str(config),
                            "--log-file", str(target), *options], cwd=tmp_path,
                            env=isolated_environment(tmp_path), capture_output=True,
                            text=True, timeout=10, check=False)
    assert result.returncode != 0
    assert "logging.file" in result.stderr
    assert config.read_text() == original
    if kind == "project":
        assert target.read_text() == "preserve project database bytes"


def test_read_only_log_destination_fails_before_readiness(executable, tmp_path):
    readonly = Path("/proc/version")
    assert readonly.is_file(), "native REST daemon tests require Linux procfs"
    original = readonly.read_bytes()
    config = tmp_path / "server.yaml"
    result = subprocess.run([str(executable), "serve", "--server-config", str(config),
        "--log-file", str(readonly)], cwd=tmp_path, env=isolated_environment(tmp_path),
        capture_output=True, text=True, timeout=10, check=False)
    assert result.returncode != 0
    assert "log" in result.stderr.lower()
    assert not config.exists()
    assert readonly.read_bytes() == original
