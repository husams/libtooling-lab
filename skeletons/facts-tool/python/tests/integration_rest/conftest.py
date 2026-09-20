import os
from pathlib import Path

import pytest
from support.domain_fixture import domain_rest  # noqa: F401
from support.native_cli_helpers import tool
from support.rest_server import native_server

pytest.importorskip("httpx")


@pytest.fixture
def rest_url(tmp_path: Path):
    try:
        tool()
    except RuntimeError:
        if os.environ.get("FACTS_TOOL_NATIVE"):
            raise
        pytest.skip("build facts-tool or set FACTS_TOOL_NATIVE")
    with native_server(tmp_path / "server") as url:
        yield url
