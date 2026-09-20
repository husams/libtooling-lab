"""Optional HTTPX-backed REST clients: install facts-tool-query[rest]."""

try:
    from .async_client import AsyncClient as LegacyAsyncClient
    from .client import Client as LegacyClient
    from .v2.async_client import AsyncClient
    from .v2.client import Client
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
    "ApiError",
    "AsyncClient",
    "Client",
    "DomainJob",
    "FileSelector",
    "IndexStatus",
    "Job",
    "JobFailedError",
    "OperationError",
    "Symbol",
    "SymbolPage",
    "JobTimeoutError",
    "ProtocolError",
    "TransportError",
]

from .v2.models import *  # noqa: F403
from .v2.models import __all__ as _v2_exports

__all__ += ["LegacyClient", "LegacyAsyncClient", *_v2_exports]
