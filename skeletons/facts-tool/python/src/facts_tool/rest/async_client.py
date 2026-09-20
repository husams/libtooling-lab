"""Native async access to REST operations, with cancellation-aware polling."""

from types import TracebackType
from typing import Self

import httpx

from .arguments import arguments, command_path, identifier
from .configuration import auth_headers, base_address, seconds
from .decoding import command_catalog, job, object_list
from .models import Job
from .polling import async_wait, validate_wait
from .transport import async_request


class AsyncClient:
    """A reusable async HTTP client; await aclose or use ``async with``."""

    def __init__(
        self, base_url: str, *, token: str | None = None, timeout: float = 10.0,
        transport: httpx.AsyncBaseTransport | None = None,
    ) -> None:
        self._http = httpx.AsyncClient(
            base_url=base_address(base_url), headers=auth_headers(token),
            timeout=seconds(timeout, "timeout"), transport=transport,
            follow_redirects=False, trust_env=False,
        )

    async def __aenter__(self) -> Self:
        await self._http.__aenter__()
        return self

    async def __aexit__(
        self, exc_type: type[BaseException] | None, exc: BaseException | None,
        traceback: TracebackType | None,
    ) -> None:
        await self._http.__aexit__(exc_type, exc, traceback)

    async def aclose(self) -> None:
        await self._http.aclose()

    async def health(self) -> dict[str, object]:
        return await async_request(self._http, "GET", "health")

    async def commands(self) -> list[dict[str, object]]:
        return command_catalog(await async_request(self._http, "GET", "v1/commands"))

    async def openapi(self) -> dict[str, object]:
        return await async_request(self._http, "GET", "openapi.json")

    async def watch_status(self) -> dict[str, object]:
        return await async_request(self._http, "GET", "v1/watch")

    async def shutdown(self) -> dict[str, object]:
        return await async_request(self._http, "POST", "v1/shutdown")

    async def list_jobs(self) -> list[Job]:
        body = await async_request(self._http, "GET", "v1/jobs")
        return [job(value, metadata=True) for value in object_list(body, "jobs")]

    async def get_job(self, job_id: str) -> Job:
        path = "v1/jobs/" + identifier(job_id)
        return job(await async_request(self._http, "GET", path))

    async def cancel_job(self, job_id: str) -> Job:
        path = "v1/jobs/" + identifier(job_id)
        return job(await async_request(self._http, "DELETE", path))

    async def submit(self, *argv: str) -> Job:
        return job(await async_request(self._http, "POST", "v1/jobs", arguments(argv)))

    async def command(self, path: str, *argv: str) -> Job:
        path = command_path(path)
        body = arguments(argv, prefix_count=len(path.split("/")))
        return job(await async_request(self._http, "POST", "v1/commands/" + path, body))

    async def wait(
        self, job_id: str, *, timeout: float | None = None, poll_interval: float = 0.1,
    ) -> Job:
        return await async_wait(self._http, job_id, timeout, poll_interval)

    async def run(
        self, *argv: str, timeout: float | None = None, poll_interval: float = 0.1,
        check: bool = True,
    ) -> Job:
        validate_wait(timeout, poll_interval)
        submitted = await self.submit(*argv)
        result = await self.wait(
            submitted.id, timeout=timeout, poll_interval=poll_interval,
        )
        return result.raise_for_status() if check else result
