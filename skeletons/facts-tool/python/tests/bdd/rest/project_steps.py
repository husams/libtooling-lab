import json
import os
import sqlite3
import time
from contextlib import ExitStack

import pytest
from pytest_bdd import given
from support.native_cli_helpers import compiler


@given("a C++ project with an answer function", target_fixture="project")
def cpp_project(tmp_path):
    try:
        target = compiler()
    except RuntimeError:
        if os.environ.get("FACTS_CLANGXX") or os.environ.get("FACTS_TOOL_COMPILER"):
            raise
        pytest.skip("set FACTS_CLANGXX to a target clang++ driver")
    source = tmp_path / "sample.cpp"
    source.write_text("int answer() { return 42; }\nint main() { return answer(); }\n")
    commands = [
        {
            "directory": str(tmp_path),
            "file": str(source),
            "arguments": [str(target), "-std=c++17", "-c", str(source)],
        }
    ]
    (tmp_path / "compile_commands.json").write_text(json.dumps(commands))
    return {
        "root": tmp_path,
        "source": source,
        "db": tmp_path / "project.db",
        "facts": tmp_path / "facts.db",
    }


def import_arguments(project):
    return (
        "import",
        "-p",
        str(project["root"]),
        "-c",
        str(project["db"]),
        "--facts",
        str(project["facts"]),
    )


@given("the project was imported through the SDK")
def imported_project(sdk, project):
    assert sdk["call"]("run", *import_arguments(project), timeout=10).succeeded


@pytest.fixture
def database_locks():
    with ExitStack() as stack:
        yield stack


@given("another connection holds the project database lock")
def locked_project(project, world, database_locks):
    connection = sqlite3.connect(project["db"])
    database_locks.callback(connection.close)
    connection.execute("BEGIN EXCLUSIVE")
    world["lock"] = connection


def running_job(sdk, identifier):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        job = sdk["call"]("get_job", identifier)
        if job.state == "running":
            return job
        assert not job.done, job
        time.sleep(0.01)
    raise AssertionError("import did not enter the running state")


@given("an import job is running and waiting for the database lock")
def blocked_import(sdk, project, world):
    job = sdk["call"]("submit", *import_arguments(project))
    world["blocked"] = running_job(sdk, job.id)
