import asyncio
import json

import httpx
import pytest

from facts_tool.rest import AsyncClient, Client, Job

from .helpers import response_for


@pytest.mark.parametrize("asynchronous", [False, True])
def test_every_endpoint_preserves_arguments_and_authentication(asynchronous):
    requests = []

    def handle(request):
        requests.append(request)
        return response_for(request)

    async def exercise(client):
        async def call(method, *args):
            value = getattr(client, method)(*args)
            return await value if asynchronous else value

        assert await call("health") == {"status": "ok"}
        assert (await call("openapi"))["openapi"] == "3.1.0"
        assert (await call("commands"))[0]["path"] == "symbol/list"
        assert (await call("watch_status"))["backend"] == "inotify"
        assert isinstance((await call("list_jobs"))[0], Job)
        assert (await call("get_job", "42")).id == "42"
        assert (await call("cancel_job", "42")).id == "42"
        argv = ("match", '--matcher=callExpr("λ")', "$(touch /tmp/x); | ' ")
        assert isinstance(await call("submit", *argv), Job)
        assert isinstance(await call("command", "symbol/list", "--help"), Job)
        assert await call("shutdown") == {"status": "stopping"}
        assert json.loads(requests[-3].content) == {"arguments": list(argv)}
        assert json.loads(requests[-2].content) == {"arguments": ["--help"]}

    transport = httpx.MockTransport(handle)
    if asynchronous:
        async def run():
            async with AsyncClient("http://localhost:1234", token="test-secret",
                                   transport=transport) as client:
                await exercise(client)
        asyncio.run(run())
    else:
        with Client("http://localhost:1234", token="test-secret",
                    transport=transport) as client:
            asyncio.run(exercise(client))
    assert [(r.method, r.url.path) for r in requests] == [
        ("GET", "/health"), ("GET", "/openapi.json"),
        ("GET", "/v1/commands"), ("GET", "/v1/watch"),
        ("GET", "/v1/jobs"), ("GET", "/v1/jobs/42"),
        ("DELETE", "/v1/jobs/42"), ("POST", "/v1/jobs"),
        ("POST", "/v1/commands/symbol/list"), ("POST", "/v1/shutdown"),
    ]
    assert all(r.headers["Authorization"] == "Bearer test-secret" for r in requests)


def test_unauthenticated_client_has_no_authorization_header(monkeypatch):
    monkeypatch.setenv("FACTS_TOOL_API_TOKEN", "unrelated-environment-token")

    def handle(request):
        assert "Authorization" not in request.headers
        return response_for(request)

    with Client("http://localhost:1234/", transport=httpx.MockTransport(handle)) as api:
        assert api.health()["status"] == "ok"
