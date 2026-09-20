"""Repository changes include clone registration and activation atomically."""

from collections.abc import Sequence

import httpx

from .async_resource import MutableResource
from .catalog_models import Clone, NewClone, Repository
from .codec import UNSET, Unset


class Repositories(MutableResource[Repository]):
    def __init__(self, http: httpx.AsyncClient) -> None:
        super().__init__(http, "repositories", Repository)

    async def create(
        self, *, name: str, clones: Sequence[NewClone], remote_url: str | None = None
    ) -> Repository:
        body: dict[str, object] = {"name": name, "clones": list(clones)}
        if remote_url is not None:
            body["remote_url"] = remote_url
        return await self._create(body)

    async def update(
        self,
        identifier: str,
        *,
        name: str | Unset = UNSET,
        remote_url: str | None | Unset = UNSET,
        clones: Sequence[Clone | NewClone] | Unset = UNSET,
        active_clone_id: str | Unset = UNSET,
    ) -> Repository:
        return await self._update(
            identifier,
            {
                "name": name,
                "remote_url": remote_url,
                "clones": clones if isinstance(clones, Unset) else list(clones),
                "active_clone_id": active_clone_id,
            },
        )

    async def wait_until_ready(
        self,
        identifier: str,
        *,
        timeout: float | None = None,
        poll_interval: float = 0.1,
    ) -> Repository:
        from .async_repository_wait import wait

        return await wait(self._http, identifier, timeout, poll_interval)
