import asyncio
import json

import httpx
import pytest

from facts_tool.rest import AsyncClient, FileReference, FileSelection, JobTimeoutError

from .v2_helpers import EXTRACTION, SYMBOL, job


def test_async_client_has_lazy_typed_parity_and_uses_delete():
    requests = []

    async def handle(request):
        requests.append(request)
        if request.url.path == "/api/v2/symbols":
            return httpx.Response(200, json={"items": [SYMBOL], "next_cursor": None})
        state = "cancelled" if request.method == "DELETE" else "succeeded"
        return httpx.Response(200, json=job(state, result=EXTRACTION))

    async def exercise():
        async with AsyncClient(
            "http://test", transport=httpx.MockTransport(handle)
        ) as api:
            symbols = api.symbols.find("example::Widget", match="exact")
            assert requests == []
            assert [s.qualified_name async for s in symbols] == ["example::Widget"]
            extraction = await api.extractions.create(
                selection=FileSelection([FileReference("widget.cpp")])
            )
            assert (await extraction.wait()).files_processed == 1
            assert (await extraction.cancel()).state == "cancelled"

    asyncio.run(exercise())
    assert requests[0].url.params["match"] == "exact"
    assert requests[1].method == "POST" and requests[2].method == "DELETE"
    assert json.loads(requests[1].content)["selection"]["type"] == "files"


def test_async_wait_timeout_bounds_an_inflight_request_without_delete():
    requests = []

    async def handle(request):
        requests.append(request)
        if len(requests) == 1:
            return httpx.Response(202, json=job())
        await asyncio.Event().wait()

    async def exercise():
        async with AsyncClient(
            "http://test", transport=httpx.MockTransport(handle)
        ) as api:
            extraction = await api.extractions.create(
                selection=FileSelection([FileReference("widget.cpp")])
            )
            with pytest.raises(JobTimeoutError):
                await extraction.wait(timeout=0.01)

    asyncio.run(exercise())
    assert [r.method for r in requests] == ["POST", "GET"]


def test_cancel_local_async_iteration_stops_fetch_and_preserves_connection():
    async def exercise():
        arrived = asyncio.Event()

        async def handle(request):
            if request.url.path == "/health":
                return httpx.Response(200, json={"status": "ok"})
            arrived.set()
            await asyncio.Event().wait()

        async with AsyncClient(
            "http://test", transport=httpx.MockTransport(handle)
        ) as api:
            collection = api.symbols.find("Widget")
            work = asyncio.create_task(collection.collect())
            await asyncio.wait_for(arrived.wait(), 1)
            work.cancel()
            with pytest.raises(asyncio.CancelledError):
                await work
            assert await api.health() == {"status": "ok"}

    asyncio.run(exercise())
