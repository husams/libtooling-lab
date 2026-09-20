import asyncio
import json

import httpx
import pytest

from facts_tool.rest import AsyncClient, Client, DomainJob, FileSelector, SymbolPage

from .domain_helpers import domain_record, index_record, symbol_record


@pytest.mark.parametrize("asynchronous", [False, True])
def test_resource_methods_send_typed_identity_and_decode_results(asynchronous):
    requests = []

    def handle(request):
        requests.append(request)
        if request.url.path == "/v1/symbols":
            return httpx.Response(
                200, json={"items": [symbol_record()], "next_cursor": "2"})
        if request.url.path == "/v1/index":
            return httpx.Response(200, json=index_record())
        return httpx.Response(202, json=domain_record())

    async def scenario():
        kind = AsyncClient if asynchronous else Client
        api = kind("http://localhost:1234", transport=httpx.MockTransport(handle))

        async def call(name, *args, **kwargs):
            value = getattr(api, name)(*args, **kwargs)
            return await value if asynchronous else value

        try:
            page = await call("find_symbols", "example::Widget", kind="class",
                              usr="c:@S@Widget", repo="example",
                              component="core", limit=2)
            assert isinstance(page, SymbolPage) and page.items[0].file_id == 21
            assert page.next_cursor == "2"
            assert (await call("index_status")).state == "ready"
            file = FileSelector(
                "widget.cpp", repo="example", clone="main", component="core")
            assert isinstance(await call("extract", file, force=True), DomainJob)
            await call("match", file, 'cxxRecordDecl().bind("node")',
                       traversal="IgnoreUnlessSpelledInSource", capture_source=True)
            await call("dependencies", file)
        finally:
            if asynchronous:
                await api.aclose()
            else:
                api.close()

    asyncio.run(scenario())
    assert requests[0].url.params["qualified_name"] == "example::Widget"
    assert requests[0].url.params["usr"] == "c:@S@Widget"
    assert "qualified_name=example%3A%3AWidget" in str(requests[0].url)
    assert [request.url.path for request in requests] == [
        "/v1/symbols", "/v1/index", "/v1/extractions",
        "/v1/matches", "/v1/dependencies"]
    bodies = [json.loads(request.content) for request in requests[2:]]
    assert bodies[0] == {"file": {"path": "widget.cpp", "repo": "example",
                                 "clone": "main", "component": "core"}, "force": True}
    assert bodies[1]["query"] == 'cxxRecordDecl().bind("node")'
    assert bodies[1]["capture_source"] is True
    assert set(bodies[2]) == {"file"}
    assert all("arguments" not in body for body in bodies)
