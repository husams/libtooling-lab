"""Failures at the HTTP, protocol, and command execution boundaries."""

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .models import Job


class ApiError(RuntimeError):
    """The server rejected a request, including an unfollowed redirect."""

    def __init__(self, status_code: int, message: str) -> None:
        self.status_code = status_code
        self.message = message
        super().__init__(f"HTTP {status_code}: {message}")


class TransportError(RuntimeError):
    """A request failed before receiving an HTTP response."""


class ProtocolError(ValueError):
    """A successful HTTP response did not match the API contract."""


class JobFailedError(RuntimeError):
    """A completed command failed or was cancelled; output remains on job."""

    def __init__(self, job: "Job") -> None:
        self.job = job
        self.job_id = job.id
        super().__init__(f"Job {job.id} {job.state} (exit code {job.exit_code})")


class JobTimeoutError(TimeoutError):
    """Waiting expired without cancelling the job on the server."""

    def __init__(self, job_id: str, timeout: float | None) -> None:
        self.job_id = job_id
        self.timeout = timeout
        super().__init__(f"Timed out waiting for job {job_id}; remote job continues")
