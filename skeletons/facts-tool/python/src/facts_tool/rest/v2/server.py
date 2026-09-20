"""Typed operational endpoints; readiness reports expected not-ready responses."""

import httpx

from ..transport import send
from .codec import decode
from .server_models import Health, Readiness, ServerSettings, Shutdown
from .wire import call, path, payload


class Server:
    def __init__(self, http: httpx.Client) -> None:
        self._http = http

    def health(self) -> Health:
        return decode(Health, call(self._http, "GET", path("health")))

    def readiness(self) -> Readiness:
        response = send(self._http, "GET", path("readiness"))
        if response.status_code == 503:
            return decode(Readiness, response.json())
        return decode(Readiness, payload(response))

    def settings(self) -> ServerSettings:
        return decode(ServerSettings, call(self._http, "GET", path("settings")))

    def shutdown(self) -> Shutdown:
        return decode(Shutdown, call(self._http, "POST", path("shutdown")))
