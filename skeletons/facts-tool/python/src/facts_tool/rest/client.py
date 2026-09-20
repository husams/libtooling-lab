"""Synchronous access to every REST operation and CLI command."""

from types import TracebackType
from typing import Self

import httpx

from .arguments import arguments, command_path, identifier
from .configuration import auth_headers, base_address, seconds
from .decoding import command_catalog, job, object_list
from .models import Job
from .polling import validate_wait, wait
from .transport import request


class Client:
    """A reusable HTTP client; close it or use a ``with`` block."""

    def __init__(
        self, base_url: str, *, token: str | None = None, timeout: float = 10.0,
        transport: httpx.BaseTransport | None = None,
    ) -> None:
        self._http = httpx.Client(
            base_url=base_address(base_url), headers=auth_headers(token),
            timeout=seconds(timeout, "timeout"), transport=transport,
            follow_redirects=False, trust_env=False,
        )

    def __enter__(self) -> Self:
        self._http.__enter__()
        return self

    def __exit__(
        self, exc_type: type[BaseException] | None, exc: BaseException | None,
        traceback: TracebackType | None,
    ) -> None:
        self._http.__exit__(exc_type, exc, traceback)

    def close(self) -> None:
        self._http.close()

    def health(self) -> dict[str, object]:
        return request(self._http, "GET", "health")

    def commands(self) -> list[dict[str, object]]:
        return command_catalog(request(self._http, "GET", "v1/commands"))

    def openapi(self) -> dict[str, object]:
        return request(self._http, "GET", "openapi.json")

    def watch_status(self) -> dict[str, object]:
        return request(self._http, "GET", "v1/watch")

    def shutdown(self) -> dict[str, object]:
        return request(self._http, "POST", "v1/shutdown")

    def list_jobs(self) -> list[Job]:
        body = request(self._http, "GET", "v1/jobs")
        return [job(value, metadata=True) for value in object_list(body, "jobs")]

    def get_job(self, job_id: str) -> Job:
        return job(request(self._http, "GET", "v1/jobs/" + identifier(job_id)))

    def cancel_job(self, job_id: str) -> Job:
        return job(request(self._http, "DELETE", "v1/jobs/" + identifier(job_id)))

    def submit(self, *argv: str) -> Job:
        return job(request(self._http, "POST", "v1/jobs", arguments(argv)))

    def command(self, path: str, *argv: str) -> Job:
        path = command_path(path)
        body = arguments(argv, prefix_count=len(path.split("/")))
        return job(request(self._http, "POST", "v1/commands/" + path, body))

    def wait(
        self, job_id: str, *, timeout: float | None = None, poll_interval: float = 0.1,
    ) -> Job:
        return wait(self._http, job_id, timeout, poll_interval)

    def run(
        self, *argv: str, timeout: float | None = None, poll_interval: float = 0.1,
        check: bool = True,
    ) -> Job:
        validate_wait(timeout, poll_interval)
        result = self.wait(
            self.submit(*argv).id, timeout=timeout, poll_interval=poll_interval,
        )
        return result.raise_for_status() if check else result
