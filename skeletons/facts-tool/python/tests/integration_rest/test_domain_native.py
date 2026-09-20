import asyncio

import pytest

from facts_tool.rest import AsyncClient, Client, DomainJob, FileSelector


@pytest.mark.parametrize("asynchronous", [False, True])
def test_typed_resource_sdk_uses_server_owned_databases(domain_rest, asynchronous):
    url, project = domain_rest

    async def scenario():
        api = (AsyncClient if asynchronous else Client)(url, token="integration-token")

        async def call(name, *args, **kwargs):
            result = getattr(api, name)(*args, **kwargs)
            return await result if asynchronous else result

        try:
            page = await call("find_symbols", "alpha::answer", kind="function")
            assert len(page.items) == 1
            symbol = page.items[0]
            assert symbol.repo == "alpha" and symbol.file_id > 0
            assert symbol.is_definition
            assert (await call("index_status")).symbols > 0
            selector = FileSelector("src/main.cpp", repo="alpha")
            submitted = await call("extract", selector, force=True)
            assert isinstance(submitted, DomainJob)
            result = await call("wait", submitted.id, timeout=20)
            assert result.succeeded and result.result["symbol_count"] > 0
            matched = await call("match", selector,
                                 'functionDecl(hasName("alpha::answer")).bind("symbol")')
            result = await call("wait", matched.id, timeout=20)
            assert result.succeeded and result.result["match_count"] >= 1
            dependency = await call(
                "dependencies", FileSelector(str(project.sources["alpha"])))
            result = await call("wait", dependency.id, timeout=20)
            assert result.succeeded and isinstance(result.result["edges"], list)
            assert any(job.id == matched.id for job in await call("list_jobs"))
        finally:
            if asynchronous:
                await api.aclose()
            else:
                api.close()

    asyncio.run(scenario())
