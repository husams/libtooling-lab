import asyncio
import sys

from facts_tool.rest import AsyncClient


def test_async_native_jobs_and_health(rest_url: str):
    async def scenario():
        async with AsyncClient(rest_url, token="integration-token") as client:
            jobs = await asyncio.gather(
                *[client.submit("config", "show") for _ in range(6)]
            )
            assert (await client.health())["status"] == "ok"
            results = await asyncio.gather(
                *[client.wait(job.id, timeout=10) for job in jobs]
            )
            assert all(job.succeeded and job.stdout for job in results)
            assert len({job.id for job in results}) == 6
            assert any(item["path"] == "extract" for item in await client.commands())
            assert "/v1/jobs" in (await client.openapi())["paths"]
            assert (await client.watch_status())["enabled"] is (sys.platform == "linux")
            assert len(await client.list_jobs()) == 6
            assert (await client.get_job(jobs[0].id)).done
            assert (await client.cancel_job(jobs[0].id)).succeeded
            assert (await client.run("config", "show", timeout=10)).succeeded
            job = await client.command("config/show")
            assert (await client.wait(job.id, timeout=10)).succeeded
            assert (await client.shutdown())["status"] == "stopping"

    asyncio.run(scenario())
