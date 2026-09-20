import asyncio

import httpx
import pytest

from facts_tool.rest import AsyncClient, JobFailedError, JobTimeoutError

from .helpers import job_record


def test_independent_async_requests_can_overlap():
    async def exercise():
        arrived = []
        both_arrived = asyncio.Event()

        async def handle(request):
            arrived.append(request)
            if len(arrived) == 2:
                both_arrived.set()
            await both_arrived.wait()
            return httpx.Response(200, json={"status": "ok"})

        async with AsyncClient("http://localhost:1234",
                               transport=httpx.MockTransport(handle)) as api:
            results = await asyncio.wait_for(
                asyncio.gather(api.health(), api.health()), timeout=1)
        assert results == [{"status": "ok"}] * 2

    asyncio.run(exercise())


def test_async_polling_keeps_other_requests_responsive():
    async def exercise():
        first_poll = asyncio.Event()
        health_finished = asyncio.Event()

        async def handle(request):
            if request.url.path == "/health":
                health_finished.set()
                return httpx.Response(200, json={"status": "ok"})
            first_poll.set()
            state = "succeeded" if health_finished.is_set() else "running"
            return httpx.Response(200, json=job_record(state))

        async with AsyncClient("http://localhost:1234",
                               transport=httpx.MockTransport(handle)) as api:
            task = asyncio.create_task(api.wait("42", timeout=1, poll_interval=0.001))
            await first_poll.wait()
            assert (await api.health())["status"] == "ok"
            assert (await task).succeeded

    asyncio.run(exercise())


def test_async_wait_and_run_report_job_failures_and_deadlines():
    async def exercise():
        state = "failed"

        async def handle(request):
            record = job_record() if request.method == "POST" else job_record(
                state, exit_code=2)
            return httpx.Response(200, json=record)

        async with AsyncClient("http://localhost:1234",
                               transport=httpx.MockTransport(handle)) as api:
            with pytest.raises(JobFailedError) as failed:
                await api.run("extract")
            assert failed.value.job.exit_code == 2
            assert (await api.run("extract", check=False)).state == "failed"
            state = "running"
            with pytest.raises(JobTimeoutError) as timed_out:
                await api.wait("42", timeout=0.005, poll_interval=0.001)
            assert timed_out.value.job_id == "42"

    asyncio.run(exercise())


def test_async_wait_budget_interrupts_a_stalled_http_response():
    async def exercise():
        cancelled = asyncio.Event()

        async def handle(_):
            try:
                await asyncio.Event().wait()
            finally:
                cancelled.set()

        async with AsyncClient("http://localhost:1234", timeout=10,
                               transport=httpx.MockTransport(handle)) as api:
            with pytest.raises(JobTimeoutError):
                await asyncio.wait_for(api.wait("42", timeout=0.005), timeout=1)
        assert cancelled.is_set()

    asyncio.run(exercise())
