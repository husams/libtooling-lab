"""Failures at the HTTP, protocol, and command execution boundaries."""

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .domain_models import DomainJob
    from .models import Job


class ApiError(RuntimeError):
    """The server rejected a request, including an unfollowed redirect."""

    def __init__(
        self, status_code: int, message: str, *, code: str | None = None,
        details: dict[str, object] | None = None,
    ) -> None:
        self.status_code = status_code
        self.message = message
        self.code = code
        self.details = details
        super().__init__(f"HTTP {status_code}: {message}")


class TransportError(RuntimeError):
    """A request failed before receiving an HTTP response."""


class ProtocolError(ValueError):
    """A successful HTTP response did not match the API contract."""


class JobFailedError(RuntimeError):
    """A completed command failed or was cancelled; output remains on job."""

    def __init__(self, job: "Job | DomainJob") -> None:
        self.job = job
        self.job_id = job.id
        status = f"Job {job.id} {job.state}"
        if hasattr(job, "exit_code"):
            status += f" (exit code {job.exit_code})"
        super().__init__(status)


class JobTimeoutError(TimeoutError):
    """Waiting expired without cancelling the job on the server."""

    def __init__(self, job_id: str, timeout: float | None) -> None:
        self.job_id = job_id
        self.timeout = timeout
        super().__init__(f"Timed out waiting for job {job_id}; remote job continues")
