import asyncio

import httpx
import pytest

from facts_tool.rest import ApiError, AsyncClient, Client

from .helpers import response_for

YAML = "openapi: 3.1.0\ninfo:\n  title: facts-tool REST API\n"


@pytest.mark.parametrize("asynchronous", [False, True])
def test_yaml_text_and_encoded_command_parameter(asynchronous):
    requests = []

    def handle(request):
        requests.append(request)
        if request.url.path == "/openapi.yaml":
            return httpx.Response(
                200, text=YAML, headers={"Content-Type": "application/yaml"}
            )
        return response_for(request)

    async def exercise():
        transport = httpx.MockTransport(handle)
        if asynchronous:
            async with AsyncClient("http://localhost:1234", transport=transport) as api:
                assert await api.openapi_yaml() == YAML
                await api.command("symbol/list", "--help")
        else:
            with Client("http://localhost:1234", transport=transport) as api:
                assert api.openapi_yaml() == YAML
                api.command("symbol/list", "--help")

    asyncio.run(exercise())
    assert requests[1].url.raw_path == b"/v1/commands/symbol%2Flist"


@pytest.mark.parametrize("asynchronous", [False, True])
def test_yaml_download_preserves_json_http_errors(asynchronous):
    transport = httpx.MockTransport(
        lambda _: httpx.Response(401, json={"error": "Authentication required"})
    )

    async def exercise():
        if asynchronous:
            async with AsyncClient("http://localhost:1234", transport=transport) as api:
                await api.openapi_yaml()
        else:
            with Client("http://localhost:1234", transport=transport) as api:
                api.openapi_yaml()

    with pytest.raises(ApiError, match="Authentication required") as caught:
        asyncio.run(exercise())
    assert caught.value.status_code == 401


def test_generated_yaml_request_yields_and_is_cancellable():
    async def exercise():
        arrived, cancelled = asyncio.Event(), asyncio.Event()

        async def handle(request):
            if request.url.path == "/health":
                return httpx.Response(200, json={"status": "ok"})
            arrived.set()
            try:
                await asyncio.Event().wait()
            finally:
                cancelled.set()

        async with AsyncClient(
            "http://localhost:1234", transport=httpx.MockTransport(handle)
        ) as api:
            stalled = asyncio.create_task(api.openapi_yaml())
            await asyncio.wait_for(arrived.wait(), timeout=1)
            assert await asyncio.wait_for(api.health(), timeout=1) == {"status": "ok"}
            stalled.cancel()
            with pytest.raises(asyncio.CancelledError):
                await stalled
            assert cancelled.is_set()

    asyncio.run(exercise())
