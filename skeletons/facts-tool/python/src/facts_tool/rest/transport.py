"""One HTTP request per operation; redirects and retries are never enabled."""

import httpx

from .decoding import object_value
from .errors import ApiError, ProtocolError, TransportError


def response_body(response: httpx.Response) -> dict[str, object]:
    try:
        value: object = response.json()
    except ValueError as error:
        if not response.is_success:
            raise ApiError(response.status_code, response.reason_phrase) from None
        raise ProtocolError("Server returned invalid JSON") from error
    if not response.is_success:
        message = value.get("error") if isinstance(value, dict) else None
        raise ApiError(
            response.status_code,
            message if isinstance(message, str) else response.reason_phrase,
        )
    return object_value(value)


def request(
    client: httpx.Client, method: str, path: str,
    body: dict[str, object] | None = None, *, budget: float | None = None,
) -> dict[str, object]:
    timeout = client.timeout if budget is None else min(
        budget, client.timeout.read or budget,
    )
    try:
        response = client.request(method, path, json=body, timeout=timeout)
    except httpx.HTTPError as error:
        message = f"{method} {path} failed: {type(error).__name__}"
        raise TransportError(message) from error
    return response_body(response)


async def async_request(
    client: httpx.AsyncClient, method: str, path: str,
    body: dict[str, object] | None = None, *, budget: float | None = None,
) -> dict[str, object]:
    timeout = client.timeout if budget is None else min(
        budget, client.timeout.read or budget,
    )
    try:
        response = await client.request(method, path, json=body, timeout=timeout)
    except httpx.HTTPError as error:
        message = f"{method} {path} failed: {type(error).__name__}"
        raise TransportError(message) from error
    return response_body(response)
