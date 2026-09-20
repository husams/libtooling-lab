"""Shared typed catalog transport and lazy collection helpers."""

from typing import TypeVar

import httpx

from .codec import decode
from .pages import AsyncCollection, validate_limit
from .wire import async_call, path, query

T = TypeVar("T")


class Resource[T]:
    def __init__(self, http: httpx.AsyncClient, resource: str, model: type[T]) -> None:
        self._http, self._resource, self._model = http, resource, model

    async def get(self, identifier: str) -> T:
        return decode(
            self._model,
            await async_call(self._http, "GET", path(self._resource, identifier)),
        )

    def list(
        self,
        *,
        limit: int = 50,
        repository: str | None = None,
        component: str | None = None,
    ) -> AsyncCollection[T]:
        return self._collection(
            {
                "limit": validate_limit(limit),
                "repository": repository,
                "component": component,
            }
        )

    def _collection(self, filters: dict[str, object]) -> AsyncCollection[T]:
        async def fetch(cursor: str | None) -> dict[str, object]:
            route = query(path(self._resource), {**filters, "cursor": cursor})
            return await async_call(self._http, "GET", route)

        return AsyncCollection(fetch, lambda value: decode(self._model, value))


class MutableResource(Resource[T]):
    async def delete(self, identifier: str, *, cascade: bool = False) -> None:
        route = query(
            path(self._resource, identifier), {"cascade": True} if cascade else {}
        )
        await async_call(self._http, "DELETE", route)

    async def _create(self, body: dict[str, object]) -> T:
        return decode(
            self._model,
            await async_call(self._http, "POST", path(self._resource), body),
        )

    async def _update(self, identifier: str, body: dict[str, object]) -> T:
        route = path(self._resource, identifier)
        return decode(self._model, await async_call(self._http, "PATCH", route, body))
