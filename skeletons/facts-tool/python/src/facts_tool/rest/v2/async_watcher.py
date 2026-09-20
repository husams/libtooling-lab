"""GET state, PATCH selected settings, or PUT a complete replacement."""

from collections.abc import Sequence

import httpx

from .codec import UNSET, Unset, decode
from .watch_models import WatcherSettings, WatcherStatus
from .wire import async_call, path


class Watcher:
    def __init__(self, http: httpx.AsyncClient) -> None:
        self._http = http

    async def status(self) -> WatcherStatus:
        return decode(
            WatcherStatus, await async_call(self._http, "GET", path("watcher"))
        )

    async def settings(self) -> WatcherSettings:
        return decode(
            WatcherSettings,
            await async_call(self._http, "GET", path("watcher/settings")),
        )

    async def replace_settings(self, settings: WatcherSettings) -> WatcherSettings:
        return decode(
            WatcherSettings,
            await async_call(self._http, "PUT", path("watcher/settings"), settings),
        )

    async def update_settings(
        self,
        *,
        enabled: bool | Unset = UNSET,
        debounce_ms: int | Unset = UNSET,
        exclude_repositories: Sequence[str] | Unset = UNSET,
        exclude_clones: Sequence[str] | Unset = UNSET,
        exclude_directories: Sequence[str] | Unset = UNSET,
        exclude_patterns: Sequence[str] | Unset = UNSET,
    ) -> WatcherSettings:
        body: dict[str, object] = {"enabled": enabled, "debounce_ms": debounce_ms}
        for key, value in (
            ("exclude_repositories", exclude_repositories),
            ("exclude_clones", exclude_clones),
            ("exclude_directories", exclude_directories),
            ("exclude_patterns", exclude_patterns),
        ):
            body[key] = value if isinstance(value, Unset) else list(value)
        return decode(
            WatcherSettings,
            await async_call(self._http, "PATCH", path("watcher/settings"), body),
        )
