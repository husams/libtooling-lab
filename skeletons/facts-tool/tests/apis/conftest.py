"""Run independently: pytest tests/apis --api-facts-tool /path/to/facts-tool."""
import shutil
from pathlib import Path

import pytest

from server import Server


def pytest_addoption(parser):
    group = parser.getgroup("facts-tool REST API")
    group.addoption("--api-facts-tool", type=Path, required=True)
    group.addoption("--api-compiler", default=shutil.which("clang++"))


@pytest.fixture
def executable(pytestconfig):
    return pytestconfig.getoption("--api-facts-tool").resolve()


@pytest.fixture
def compiler(pytestconfig):
    value = pytestconfig.getoption("--api-compiler")
    assert value, "--api-compiler or clang++ on PATH is required"
    return str(Path(value).resolve())


@pytest.fixture
def server_factory(executable, tmp_path):
    active = []

    def start(*options, token=None):
        server = Server(executable, tmp_path / f"server-{len(active)}", options, token)
        active.append(server)
        return server

    yield start
    for server in reversed(active):
        server.close()


@pytest.fixture
def server(server_factory):
    return server_factory()
