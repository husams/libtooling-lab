import asyncio
import os
from contextlib import ExitStack
from importlib.util import find_spec

import pytest
from pytest_bdd import given, parsers
from support.native_cli_helpers import tool
from support.rest_server import native_server

if find_spec("httpx") is None and os.environ.get("FACTS_TOOL_NATIVE"):
    raise RuntimeError("install facts-tool-query[rest] to run native REST BDD tests")
pytest.importorskip("httpx")

from facts_tool.rest import AsyncClient, Client  # noqa: E402


@pytest.fixture
def rest_url(tmp_path):
    try:
        tool()
    except RuntimeError:
        if os.environ.get("FACTS_TOOL_NATIVE"):
            raise
        pytest.skip("build facts-tool or set FACTS_TOOL_NATIVE")
    with native_server(tmp_path / "server") as url:
        yield url


@pytest.fixture
def clients(rest_url):
    with ExitStack() as stack:

        def create(kind, token="integration-token"):
            if kind == "sync":
                client = stack.enter_context(Client(rest_url, token=token))
                return {"call": lambda name, *a, **kw: getattr(client, name)(*a, **kw)}
            assert kind == "async"
            runner = stack.enter_context(asyncio.Runner())
            client = AsyncClient(rest_url, token=token)
            runner.run(client.__aenter__())
            stack.callback(lambda: runner.run(client.aclose()))
            return {
                "client": client,
                "runner": runner,
                "call": lambda name, *a, **kw: runner.run(
                    getattr(client, name)(*a, **kw)
                ),
            }

        yield create


@given("a native facts-tool REST server")
def server_running(rest_url):
    assert rest_url.startswith("http://127.0.0.1:")


@given(parsers.parse("an authenticated {kind} SDK client"), target_fixture="sdk")
def authenticated_client(clients, kind):
    return clients(kind)


@given(parsers.parse("an unauthenticated {kind} SDK client"), target_fixture="sdk")
def unauthenticated_client(clients, kind):
    return clients(kind, token="wrong-token")
