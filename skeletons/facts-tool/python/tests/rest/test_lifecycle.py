import asyncio

import httpx
import pytest

from facts_tool.rest import AsyncClient, Client

from .helpers import response_for


class ClosingTransport(httpx.MockTransport):
    def __init__(self):
        super().__init__(response_for)
        self.closed = 0

    def close(self):
        self.closed += 1

    async def aclose(self):
        self.closed += 1


def test_sync_context_releases_connections_on_error_and_close_is_idempotent():
    transport = ClosingTransport()
    with pytest.raises(LookupError), Client(
        "http://localhost:1234", transport=transport
    ) as client:
        assert client.health()["status"] == "ok"
        raise LookupError("application error")
    client.close()
    assert transport.closed == 1
    with pytest.raises(RuntimeError):
        client.health()


def test_async_context_releases_connections_on_error_and_close_is_idempotent():
    async def exercise():
        transport = ClosingTransport()
        with pytest.raises(LookupError):
            async with AsyncClient("http://localhost:1234", transport=transport) as api:
                assert (await api.health())["status"] == "ok"
                raise LookupError("application error")
        await api.aclose()
        assert transport.closed == 1
        with pytest.raises(RuntimeError):
            await api.health()

    asyncio.run(exercise())


def test_cancelling_local_wait_does_not_cancel_server_job_or_close_client():
    async def exercise():
        entered = asyncio.Event()
        methods = []

        async def handle(request):
            methods.append(request.method)
            if request.url.path.startswith("/v1/jobs/"):
                entered.set()
                await asyncio.Event().wait()
            return httpx.Response(200, json={"status": "ok"})

        async with AsyncClient("http://localhost:1234",
                               transport=httpx.MockTransport(handle)) as api:
            task = asyncio.create_task(api.wait("42"))
            await entered.wait()
            task.cancel()
            with pytest.raises(asyncio.CancelledError):
                await task
            assert (await api.health())["status"] == "ok"
        assert methods == ["GET", "GET"]

    asyncio.run(exercise())
