"""Provision real source metadata; clients receive only source identities."""
import importlib.util
import os
import time
from pathlib import Path

import pytest

from facts_tool.rest import Client
from support.native_cli_helpers import compiler, tool
from support.rest_server import native_server


@pytest.fixture
def domain_rest(tmp_path):
    source = Path(__file__).resolve().parents[3] / "tests/domain_project.py"
    spec = importlib.util.spec_from_file_location("native_domain_project", source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("FACTS_TOOL_")}
    project = module.DomainProject(
        tool(), tmp_path / "project", compiler(), environment)
    project.add("alpha")
    options = tuple(map(str, project.options()))
    with native_server(tmp_path / "server", options) as url:
        with Client(url, token="integration-token") as api:
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                state = api.index_status()
                assert state.state != "failed", state
                if state.state == "ready" and not state.pending:
                    break
                time.sleep(0.025)
            else:
                raise AssertionError("native index did not become ready")
        yield url, project
