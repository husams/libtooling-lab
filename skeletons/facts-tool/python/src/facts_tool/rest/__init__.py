"""Optional HTTPX-backed REST clients: install facts-tool-query[rest]."""

try:
    from .async_client import AsyncClient
    from .client import Client
except ModuleNotFoundError as error:
    if error.name == "httpx":
        raise ImportError(
            "REST clients require HTTPX; install facts-tool-query[rest]"
        ) from error
    raise

from .domain_models import (
    DomainJob,
    FileSelector,
    IndexStatus,
    OperationError,
    Symbol,
    SymbolPage,
)
from .errors import (
    ApiError,
    JobFailedError,
    JobTimeoutError,
    ProtocolError,
    TransportError,
)
from .models import Job

__all__ = [
    "ApiError", "AsyncClient", "Client", "DomainJob", "FileSelector", "IndexStatus",
    "Job", "JobFailedError", "OperationError", "Symbol", "SymbolPage",
    "JobTimeoutError", "ProtocolError", "TransportError",
]
