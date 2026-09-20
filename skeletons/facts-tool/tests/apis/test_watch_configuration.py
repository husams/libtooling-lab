"""Validate and migrate persisted repository monitoring settings."""
import json
import re
import sqlite3
import subprocess
import sys

import pytest
from server import Server, isolated_environment
from domain_http import index_ready
from support import eventually


@pytest.mark.parametrize("watch", [False, [], "yes", {"enabled": "invalid"},
    {"exclude_repositories": "repo"}, {"exclude_clones": [""]},
    {"exclude_directories": "build"}, {"exclude_patterns": ["\0"]}])
def test_invalid_watch_settings_rejected(executable, tmp_path, watch):
    config = tmp_path / "server.yaml"
    config.write_text(json.dumps({"watch": watch}))
    result = subprocess.run([str(executable), "serve", "--server-config", str(config)],
                            cwd=tmp_path, env=isolated_environment(tmp_path),
                            capture_output=True, text=True, timeout=10, check=False)
    assert result.returncode == 2, result
    assert "configuration error" in result.stderr
    assert not (tmp_path / "server.yaml.pid").exists()


def test_watch_and_no_watch_conflict(executable, tmp_path):
    result = subprocess.run([str(executable), "serve", "--watch", "--no-watch"],
                            cwd=tmp_path, env=isolated_environment(tmp_path),
                            capture_output=True, text=True, timeout=10, check=False)
    assert result.returncode == 2
    assert "watch" in result.stderr


@pytest.mark.parametrize("enabled,option", [(True, "--no-watch"), (False, "--watch")])
def test_cli_watch_override_is_saved(executable, tmp_path, enabled, option):
    if option == "--watch" and sys.platform != "linux":
        pytest.skip("inotify requires Linux")
    (tmp_path / "server.yaml").write_text(f"watch:\n  enabled: {str(enabled).lower()}\n")
    server = Server(executable, tmp_path, [option])
    try:
        _, state = server.api.request("GET", "/v1/watch")
        assert state["enabled"] is not enabled
        saved = server.config.read_text()
        expected = "false" if enabled else "true"
        assert re.search(rf"^  enabled: {expected}$", saved, re.MULTILINE)
    finally:
        server.close()


def test_legacy_directories_are_removed_without_being_watched(executable, tmp_path):
    legacy = json.dumps(str(tmp_path / "missing"))
    (tmp_path / "server.yaml").write_text(
        f"watch_directories:\n  - {legacy}\nwatch:\n  enabled: false\n")
    server = Server(executable, tmp_path)
    try:
        _, state = server.api.request("GET", "/v1/watch")
        assert state["directories"] == []
        assert not state["enabled"]
        assert "watch_directories" not in server.config.read_text()
    finally:
        server.close()


@pytest.mark.skipif(sys.platform != "linux", reason="inotify requires Linux")
def test_initialized_empty_catalog_does_not_invent_watched_repositories(server_factory, tmp_path):
    database = tmp_path / "future-project.db"
    server = server_factory("--watch", "--conf", database)
    index = index_ready(server.api)
    def ready():
        status, state = server.api.request("GET", "/v1/watch")
        assert status == 200
        return state if state["ready"] and not state["last_error"] else None
    state = eventually(ready)
    assert state["enabled"]
    assert state["directories"] == [] and state["clones"] == []
    assert state["cycles"] == 0 and state["latest_jobs"] == []
    assert index["files"] == 0 and index["symbols"] == 0 and database.is_file()
    with sqlite3.connect(database.as_uri() + "?mode=ro", uri=True) as connection:
        for table in ("repository", "clone", "file", "global_symbol_index"):
            assert connection.execute(f"SELECT count(*) FROM {table}").fetchone() == (0,)
