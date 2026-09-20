import asyncio

import pytest
from pytest_bdd import scenarios, then, when

from .project_steps import *  # noqa: F403

scenarios("async.feature")


@when("I poll the import alongside concurrent submissions and health requests")
def concurrent_requests(sdk, world):
    async def requests():
        client = sdk["client"]
        waiting = asyncio.create_task(client.wait(world["blocked"].id, timeout=5))
        try:
            jobs = await asyncio.wait_for(
                asyncio.gather(
                    *(client.submit("config", "show") for _ in range(4)),
                ),
                timeout=2,
            )
            health = await asyncio.wait_for(client.health(), timeout=2)
            world["responsive"] = health["status"] == "ok" and not waiting.done()
            world["queued"] = jobs
            world["lock"].rollback()
            world["completed"] = await asyncio.gather(
                waiting,
                *(client.wait(job.id, timeout=5) for job in jobs),
            )
        finally:
            waiting.cancel()
            await asyncio.gather(waiting, return_exceptions=True)

    sdk["runner"].run(requests())


@then("health responds during polling and all independent jobs complete")
def independent_results(world):
    assert world["responsive"]
    assert len({job.id for job in world["queued"]}) == 4
    assert all(job.state == "queued" for job in world["queued"])
    assert all(job.succeeded for job in world["completed"])


@when("I cancel the asyncio task polling the blocked import")
def cancelled_polling_task(sdk, world):
    async def cancel_task():
        client = sdk["client"]
        waiting = asyncio.create_task(client.wait(world["blocked"].id, timeout=5))
        await client.health()
        assert not waiting.done()
        waiting.cancel()
        with pytest.raises(asyncio.CancelledError):
            await waiting
        world["after_cancel"] = await client.get_job(world["blocked"].id)

    sdk["runner"].run(cancel_task())


@then("only local polling stops and the remote import can still finish")
def local_cancellation_only(sdk, world):
    assert world["after_cancel"].state == "running"
    world["lock"].rollback()
    assert sdk["call"]("wait", world["blocked"].id, timeout=5).succeeded
