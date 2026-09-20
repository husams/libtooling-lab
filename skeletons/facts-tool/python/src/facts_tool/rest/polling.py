"""Poll snapshots with a monotonic budget, without cancelling remote work."""

import asyncio
import time

import httpx

from .arguments import identifier
from .configuration import seconds
from .domain_models import DomainJob
from .endpoints import endpoint
from .errors import JobTimeoutError, TransportError
from .models import Job
from .snapshots import snapshot
from .transport import async_request, request


def validate_wait(timeout: float | None, poll_interval: float) -> None:
    seconds(poll_interval, "poll_interval")
    if timeout is not None:
        seconds(timeout, "timeout", allow_zero=True)


def remaining(stop: float | None, job_id: str, timeout: float | None) -> float | None:
    if stop is None:
        return None
    budget = stop - time.monotonic()
    if budget <= 0:
        raise JobTimeoutError(job_id, timeout)
    return budget


def wait(
    client: httpx.Client, job_id: str, timeout: float | None, poll_interval: float,
) -> Job | DomainJob:
    method, path = endpoint("getJob", id=identifier(job_id))
    validate_wait(timeout, poll_interval)
    stop = None if timeout is None else time.monotonic() + timeout
    while True:
        budget = remaining(stop, job_id, timeout)
        try:
            result = snapshot(request(client, method, path, budget=budget))
        except TransportError:
            remaining(stop, job_id, timeout)
            raise
        budget = remaining(stop, job_id, timeout)
        if result.done:
            return result
        time.sleep(poll_interval if budget is None else min(poll_interval, budget))


async def async_wait(
    client: httpx.AsyncClient, job_id: str, timeout: float | None, poll_interval: float,
) -> Job | DomainJob:
    method, path = endpoint("getJob", id=identifier(job_id))
    validate_wait(timeout, poll_interval)
    stop = None if timeout is None else time.monotonic() + timeout
    while True:
        budget = remaining(stop, job_id, timeout)
        try:
            async with asyncio.timeout(budget):
                result = snapshot(
                    await async_request(client, method, path, budget=budget))
        except TimeoutError as error:
            raise JobTimeoutError(job_id, timeout) from error
        except TransportError:
            remaining(stop, job_id, timeout)
            raise
        budget = remaining(stop, job_id, timeout)
        if result.done:
            return result
        pause = poll_interval if budget is None else min(poll_interval, budget)
        await asyncio.sleep(pause)
