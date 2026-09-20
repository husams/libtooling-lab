"""Typed operational endpoints; readiness reports expected not-ready responses."""

import httpx

from ..transport import async_send
from .codec import decode
from .server_models import Health, Readiness, ServerSettings, Shutdown
from .wire import async_call, path, payload


class Server:
    def __init__(self, http: httpx.AsyncClient) -> None:
        self._http = http

    async def health(self) -> Health:
        return decode(Health, await async_call(self._http, "GET", path("health")))

    async def readiness(self) -> Readiness:
        response = await async_send(self._http, "GET", path("readiness"))
        if response.status_code == 503:
            return decode(Readiness, response.json())
        return decode(Readiness, payload(response))

    async def settings(self) -> ServerSettings:
        return decode(
            ServerSettings, await async_call(self._http, "GET", path("settings"))
        )

    async def shutdown(self) -> Shutdown:
        return decode(Shutdown, await async_call(self._http, "POST", path("shutdown")))
