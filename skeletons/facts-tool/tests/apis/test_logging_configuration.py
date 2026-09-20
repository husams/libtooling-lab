"""Logging configuration is strict, anchored, overridable and restartable."""
import json
import subprocess

import pytest
import yaml
from server import Server, isolated_environment


@pytest.mark.parametrize("logging", [None, False, [], "debug", 2,
    {"unknown": True}, {"level": "verbose"}, {"level": 3}, {"level": None},
    {"file": None}, {"file": []}, {"file": False}, {"file": 2}, {"file": "bad\0name"},
    {"verbosity": -1}, {"verbosity": 4}, {"verbosity": "debug"},
    {"verbosity": False}, {"verbosity": None}, {"verbosity": 1.5}, {"verbosity": "2"},
    {"level": "info", "verbosity": 1}])
def test_invalid_logging_rejected_before_pid_lock(executable, tmp_path, logging):
    config = tmp_path / "server.yaml"
    config.write_text(json.dumps({"logging": logging}))
    result = subprocess.run([str(executable), "serve", "--server-config", str(config)],
                            cwd=tmp_path, env=isolated_environment(tmp_path),
                            capture_output=True, text=True, timeout=10, check=False)
    assert result.returncode == 2, result
    assert "logging" in result.stderr.lower(), result.stderr
    assert not config.with_suffix(".yaml.pid").exists()


@pytest.mark.parametrize("options", [("--log-level", "verbose"),
    ("--verbose", "4"), ("-v", "-1"), ("--verbose", "1.5"),
    ("--log-level", "debug", "-v", "2")])
def test_invalid_cli_logging_rejected(executable, tmp_path, options):
    result = subprocess.run([str(executable), "serve", *options], cwd=tmp_path,
                            env=isolated_environment(tmp_path), capture_output=True,
                            text=True, timeout=10, check=False)
    assert result.returncode == 2, result
    assert not (tmp_path / "server.yaml").exists()


@pytest.mark.parametrize("cli", [False, True])
def test_log_paths_use_yaml_directory_or_cli_working_directory(executable, tmp_path, cli):
    root = tmp_path / "configuration"
    root.mkdir()
    (root / "server.yaml").write_text("logging:\n  file: log.jsonl\n  level: info\n")
    options = ["--log-file", "override.jsonl", "--log-level", "trace"] if cli else []
    server = Server(executable, root, options, cwd=tmp_path)
    try:
        expected = tmp_path / "override.jsonl" if cli else root / "log.jsonl"
        saved = yaml.safe_load(server.config.read_text())["logging"]
        assert saved["file"] == str(expected)
        assert saved["level"] == ("trace" if cli else "info")
        assert expected.exists()
        assert not (tmp_path / "log.jsonl").exists()
        if cli:
            assert not (root / "log.jsonl").exists()
    finally:
        server.close()


@pytest.mark.parametrize("verbosity,level", [(0, "error"), (1, "info"),
                                           (2, "debug"), (3, "trace")])
def test_yaml_numeric_verbosity_persists_canonical_level(executable, tmp_path,
                                                       verbosity, level):
    (tmp_path / "server.yaml").write_text(f"logging:\n  verbosity: {verbosity}\n")
    server = Server(executable, tmp_path)
    try:
        saved = yaml.safe_load(server.config.read_text())["logging"]
        assert saved["level"] == level and "verbosity" not in saved
    finally:
        server.close()
