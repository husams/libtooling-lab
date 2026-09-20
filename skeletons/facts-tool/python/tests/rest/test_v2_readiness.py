import asyncio

import httpx
import pytest

from facts_tool.rest import AsyncClient, Client, RepositoryNotReady

from .v2_helpers import INDEX, REPOSITORY, WATCH


@pytest.mark.parametrize("asynchronous", [False, True])
@pytest.mark.parametrize("resumed", [False, True])
def test_ready_waits_for_registered_clone_and_completed_index(asynchronous, resumed):
    watchers = []

    def handle(request):
        if request.url.path == "/api/v2/watcher":
            watchers.append(request)
            baseline = {**WATCH, "cycles": 0 if resumed else 1, "resumed": resumed}
            value = {**baseline, "clones": []} if len(watchers) == 1 else baseline
        elif request.url.path == "/api/v2/index":
            value = INDEX
        else:
            value = REPOSITORY
        return httpx.Response(200, json=value)

    async def exercise():
        transport = httpx.MockTransport(handle)
        if asynchronous:
            async with AsyncClient("http://test", transport=transport) as api:
                repo = await api.repositories.wait_until_ready(
                    "repo-1", timeout=1, poll_interval=0.001
                )
        else:
            with Client("http://test", transport=transport) as api:
                repo = api.repositories.wait_until_ready(
                    "repo-1", timeout=1, poll_interval=0.001
                )
        assert repo.indexed_source_count == 1

    asyncio.run(exercise())
    assert len(watchers) == 2


@pytest.mark.parametrize(
    "enabled,source_count,code",
    [
        (False, 1, "automatic_import_disabled"),
        (True, 0, "needs_configuration"),
    ],
)
def test_ready_reports_missing_configuration_instead_of_false_success(
    enabled,
    source_count,
    code,
):
    def handle(request):
        if request.url.path == "/api/v2/watcher":
            value = {**WATCH, "enabled": enabled}
        elif request.url.path == "/api/v2/index":
            value = INDEX
        else:
            value = {**REPOSITORY, "source_count": source_count}
        return httpx.Response(200, json=value)

    with Client("http://test", transport=httpx.MockTransport(handle)) as api:
        with pytest.raises(RepositoryNotReady) as caught:
            api.repositories.wait_until_ready("repo-1", timeout=1)
        assert caught.value.code == code
