"""Wait for automatic import, extraction, and global index publication."""

import asyncio
import time

import httpx

from ..async_client import AsyncClient as LegacyClient
from ..polling import validate_wait
from .async_index import Index
from .async_watcher import Watcher
from .catalog_models import Repository
from .readiness import RepositoryTimeout, monitoring_ready, sources_ready


async def wait(
    http: httpx.AsyncClient,
    repository_id: str,
    timeout: float | None,
    poll_interval: float,
) -> Repository:
    from .async_repositories import Repositories

    validate_wait(timeout, poll_interval)
    stop = None if timeout is None else time.monotonic() + timeout
    legacy = LegacyClient.__new__(LegacyClient)
    legacy._http = http
    while True:
        if stop is not None and time.monotonic() >= stop:
            raise RepositoryTimeout(f"Timed out waiting for repository {repository_id}")
        repo = await Repositories(http).get(repository_id)
        watch = await Watcher(http).status()
        index = await Index(http).status()
        if monitoring_ready(repo, watch, index):
            jobs = [
                await legacy.get_job(identifier) for identifier in watch.latest_jobs
            ]
            for job in jobs:
                if job.done:
                    job.raise_for_status()
            if all(job.done for job in jobs) and sources_ready(repo):
                return repo
        pause = (
            poll_interval
            if stop is None
            else min(poll_interval, max(0, stop - time.monotonic()))
        )
        await asyncio.sleep(pause)
