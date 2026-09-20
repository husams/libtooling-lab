"""HTTP transport for v2: bodies only on writes, typed HTTP failures."""

from urllib.parse import quote, urlencode

import httpx

from ..domain_requests import text
from ..errors import ApiError
from ..transport import async_send, response_body, send
from .codec import encode


class ResourceNotFound(ApiError):
    """The addressed resource no longer exists."""


class ResourceConflict(ApiError):
    """The update conflicts with server state."""


class AmbiguousFile(ResourceConflict):
    """A file path needs an additional repository or component selector."""


class ValidationError(ApiError):
    """A request violates its typed resource contract."""


def path(resource: str, identifier: str | None = None) -> str:
    suffix = "" if identifier is None else "/" + quote(text(identifier, "id"), safe="")
    return "/api/v2/" + resource + suffix


def query(route: str, values: dict[str, object]) -> str:
    params = {
        k: str(v).lower() if isinstance(v, bool) else str(v)
        for k, v in values.items()
        if v is not None
    }
    return route + ("?" + urlencode(params) if params else "")


def payload(response: httpx.Response) -> dict[str, object]:
    if response.status_code == 204:
        return {}
    try:
        return response_body(response)
    except ApiError as error:
        error_type = (
            AmbiguousFile
            if error.code == "ambiguous_file"
            else {
                404: ResourceNotFound,
                409: ResourceConflict,
                422: ValidationError,
            }.get(error.status_code, ApiError)
        )
        raise error_type(
            error.status_code, error.message, code=error.code, details=error.details
        ) from error


def call(
    client: httpx.Client,
    method: str,
    route: str,
    body: object = None,
    *,
    budget: float | None = None,
) -> dict[str, object]:
    return payload(send(client, method, route, encode(body), budget=budget))


async def async_call(
    client: httpx.AsyncClient,
    method: str,
    route: str,
    body: object = None,
    *,
    budget: float | None = None,
) -> dict[str, object]:
    return payload(await async_send(client, method, route, encode(body), budget=budget))
