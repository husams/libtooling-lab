import asyncio

import httpx
import pytest

from facts_tool.rest import ApiError, AsyncClient, Client, DomainJob, JobFailedError

from .domain_helpers import domain_record


@pytest.mark.parametrize("asynchronous", [False, True])
def test_wait_decodes_structured_domain_completion(asynchronous):
    states = iter(["queued", "running", "succeeded"])

    def handle(_):
        state = next(states)
        result = {"symbol_count": 3} if state == "succeeded" else None
        return httpx.Response(200, json=domain_record(state, result=result))

    async def scenario():
        transport = httpx.MockTransport(handle)
        if asynchronous:
            async with AsyncClient("http://localhost:1234", transport=transport) as api:
                return await api.wait("domain-7", poll_interval=0.001)
        with Client("http://localhost:1234", transport=transport) as api:
            return api.wait("domain-7", poll_interval=0.001)

    result = asyncio.run(scenario())
    assert isinstance(result, DomainJob) and result.succeeded and result.done
    assert result.result == {"symbol_count": 3}
    assert result.raise_for_status() is result


def test_domain_failure_and_cancellation_are_structured():
    error = {"code": "file_not_found", "message": "Source is not registered",
             "details": {"diagnostics": [
                 {"severity": "error", "message": "Missing file"}]}}
    transport = httpx.MockTransport(lambda _: httpx.Response(
        200, json=domain_record("failed", error=error)))
    with Client("http://localhost:1234", transport=transport) as api:
        failed = api.get_job("domain-7")
    assert isinstance(failed, DomainJob) and failed.error.code == "file_not_found"
    assert failed.error.details["diagnostics"][0]["severity"] == "error"
    with pytest.raises(JobFailedError) as caught:
        failed.raise_for_status()
    assert caught.value.job is failed


def test_domain_http_error_preserves_code_message_and_status():
    transport = httpx.MockTransport(lambda _: httpx.Response(409, json={"error": {
        "code": "cannot_cancel_running", "message": "Operation must finish safely"}}))
    with (Client("http://localhost:1234", transport=transport) as api,
          pytest.raises(ApiError) as caught):
        api.cancel_job("domain-7")
    assert caught.value.status_code == 409
    assert caught.value.code == "cannot_cancel_running"
    assert caught.value.message == "Operation must finish safely"


def test_async_resource_request_does_not_block_health_and_can_be_cancelled():
    async def scenario():
        started, cancelled = asyncio.Event(), asyncio.Event()

        async def handle(request):
            if request.url.path == "/health":
                return httpx.Response(200, json={"status": "ok"})
            started.set()
            try:
                await asyncio.Event().wait()
            finally:
                cancelled.set()

        async with AsyncClient("http://localhost:1234",
                               transport=httpx.MockTransport(handle)) as api:
            search = asyncio.create_task(api.find_symbols("example::Widget"))
            await asyncio.wait_for(started.wait(), timeout=1)
            assert (await asyncio.wait_for(api.health(), timeout=1))["status"] == "ok"
            search.cancel()
            with pytest.raises(asyncio.CancelledError):
                await search
            assert cancelled.is_set()
    asyncio.run(scenario())
