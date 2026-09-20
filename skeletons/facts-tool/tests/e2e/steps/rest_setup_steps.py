"""Shared real-server and real-project Given steps."""
import subprocess
import sys

import pytest
from pytest_bdd import given
from support.rest_http import eventually
from support.rest_project import create_project, watch_status
from support.rest_server import RestServer


@pytest.fixture
def rest_server(pytestconfig, tmp_path):
    server = RestServer(pytestconfig.getoption("--facts-tool"), tmp_path / "server")
    yield server
    server.close()


@given("a running authenticated native REST server")
def running_server(rest_server):
    rest_server.start()


@given("a running authenticated native REST daemon")
def daemon(rest_server):
    rest_server.start(["--daemon"])


@given("a real C++ project imported and extracted through REST")
def project(rest_server, pytestconfig):
    root = rest_server.root / "project"
    rest_server.source, rest_server.header = create_project(
        root, pytestconfig.getoption("--compiler"))
    rest_server.project = root
    rest_server.api.run(["-c", str(root / "project.db"), "-p", str(root)], "import")
    rest_server.api.run(["-c", str(root / "project.db"), "-o", str(root / "facts.db")],
                        "extract")


@given("a native inotify server watching an indexed C++ project")
def watched_project(rest_server, pytestconfig, cache=False):
    if sys.platform != "linux":
        pytest.skip("native inotify requires Linux")
    root = rest_server.root / "project"
    rest_server.source, rest_server.header = create_project(
        root, pytestconfig.getoption("--compiler"))
    rest_server.project = root
    defaults = rest_server.root / "defaults.yaml"
    defaults.write_text(f"facts_template: {root / 'facts.db'}\nast_cache: {str(cache).lower()}\n")
    if cache:
        subprocess.run(["git", "init", "-q", str(root)], check=True)
        subprocess.run(["git", "-C", str(root), "add", "."], check=True)
        subprocess.run(["git", "-C", str(root), "-c", "user.name=REST BDD",
                        "-c", "user.email=bdd@example.invalid", "commit", "-qm", "fixture"],
                       check=True)
        rest_server.commit = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"])
    rest_server.start(["--watch", "--conf", root / "project.db",
                       "--config", defaults, "--debounce-ms", "50"])
    imported = rest_server.api.run(["-p", str(root), "-v", "2"], "import")
    extracted = rest_server.api.run(["-v", "2"], "extract")
    if cache:
        assert "ast-cache: stored" in imported["stderr"]
        assert "ast-cache: hit" in extracted["stderr"]
    state = eventually(lambda: (status if str(root) in status["directories"] else None)
                       if (status := watch_status(rest_server)) else None)
    assert state["backend"] == "inotify" and state["enabled"]
    assert str(root) in state["directories"]


@given("a native inotify server watching an indexed project with a cached AST")
def watched_cached_project(rest_server, pytestconfig):
    watched_project(rest_server, pytestconfig, cache=True)
