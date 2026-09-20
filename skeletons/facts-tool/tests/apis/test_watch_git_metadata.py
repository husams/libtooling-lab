"""Git metadata watches recover when the info directory is replaced."""
import shutil
import subprocess
import sys
import time

import pytest
from project import create_project
from support import eventually
from watch_support import symbols, wait_cycle, watch_status

pytestmark = pytest.mark.skipif(sys.platform != "linux", reason="inotify needs Linux")


def test_recreated_git_info_directory_keeps_exclude_rules_live(
        server_factory, compiler, tmp_path):
    root = tmp_path / "project"
    source, _ = create_project(root, compiler)
    subprocess.run(["git", "init", "-q", str(root)], check=True)
    config = tmp_path / "defaults.yaml"
    config.write_text(f"facts_template: {root / 'facts.db'}\n")
    server = server_factory("--watch", "--conf", root / "project.db",
                            "--config", config, "--debounce-ms", "50")
    server.api.run(["-p", str(root)], "import")
    server.api.run([], "extract")
    eventually(lambda: str(root) in watch_status(server.api)["directories"])
    info = root / ".git" / "info"
    previous = watch_status(server.api)["cycles"]
    (info / "exclude").write_text(f"/{source.name}\n")
    wait_cycle(server.api, previous)

    previous = watch_status(server.api)["cycles"]
    shutil.rmtree(info)
    wait_cycle(server.api, previous)
    previous = watch_status(server.api)["cycles"]
    info.mkdir()
    (info / "exclude").write_text(f"/{source.name}\n")
    wait_cycle(server.api, previous)
    # Allow the directory-creation and close-write events to finish debouncing.
    time.sleep(0.2)
    previous = watch_status(server.api)["cycles"]
    source.write_text(source.read_text() + "int metadata_recovered() { return 9; }\n")
    time.sleep(0.3)
    assert watch_status(server.api)["cycles"] == previous
    assert "metadata_recovered" not in symbols(server.api)

    (info / "exclude").write_text("")
    wait_cycle(server.api, previous)
    assert "metadata_recovered" in symbols(server.api)
    assert server.api.request("GET", "/health")[0] == 200
