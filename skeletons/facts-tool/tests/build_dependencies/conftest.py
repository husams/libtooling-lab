"""Compile real API dependencies without LLVM or network access."""
import shutil
from pathlib import Path

import pytest
from support import Consumer


def pytest_addoption(parser):
    group = parser.getgroup("API build dependencies")
    group.addoption("--dependency-cmake", default=shutil.which("cmake"))
    group.addoption("--dependency-cxx", default=shutil.which("c++"))
    group.addoption("--boost-headers", type=Path, default=Path("/usr/include"))
    group.addoption("--json-headers", type=Path, default=Path("/usr/include"))


@pytest.fixture
def consumer(pytestconfig, tmp_path):
    options = pytestconfig.getoption
    return Consumer(
        tmp_path, options("--dependency-cmake"), options("--dependency-cxx"),
        options("--boost-headers"), options("--json-headers"),
    )


@pytest.fixture
def sources(consumer, tmp_path):
    boost = tmp_path / "boost source"
    json = tmp_path / "json source"
    boost.mkdir()
    (json / "include").mkdir(parents=True)
    (boost / "boost").symlink_to(consumer.boost / "boost", target_is_directory=True)
    (json / "include" / "nlohmann").symlink_to(
        consumer.json / "nlohmann", target_is_directory=True,
    )
    return boost, json
