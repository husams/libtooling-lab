"""Replayable lazy collections, retaining only one bounded page at a time."""

from collections.abc import AsyncIterator, Awaitable, Callable, Iterator
from typing import TypeVar

from ..errors import ProtocolError

T = TypeVar("T")


def validate_limit(limit: int) -> int:
    if type(limit) is not int or not 1 <= limit <= 1000:
        raise ValueError("limit must be an integer from 1 through 1000")
    return limit


def page[T](
    body: dict[str, object], parser: Callable[[object], T]
) -> tuple[list[T], str | None]:
    items, cursor = body.get("items"), body.get("next_cursor")
    if not isinstance(items, list) or not (cursor is None or isinstance(cursor, str)):
        raise ProtocolError("Invalid collection page")
    if "next_cursor" not in body:
        raise ProtocolError("Missing page cursor")
    return [parser(item) for item in items], cursor


class Collection[T]:
    def __init__(
        self,
        fetch: Callable[[str | None], dict[str, object]],
        parser: Callable[[object], T],
    ) -> None:
        self._fetch, self._parser = fetch, parser

    def __iter__(self) -> Iterator[T]:
        cursor, seen = None, set[str]()
        while True:
            items, following = page(self._fetch(cursor), self._parser)
            yield from items
            if following is None:
                return
            if following in seen:
                raise ProtocolError("Server repeated a pagination cursor")
            seen.add(following)
            cursor = following

    def collect(self) -> list[T]:
        return list(self)


class AsyncCollection[T]:
    def __init__(
        self,
        fetch: Callable[[str | None], Awaitable[dict[str, object]]],
        parser: Callable[[object], T],
    ) -> None:
        self._fetch, self._parser = fetch, parser

    async def __aiter__(self) -> AsyncIterator[T]:
        cursor, seen = None, set[str]()
        while True:
            items, following = page(await self._fetch(cursor), self._parser)
            for item in items:
                yield item
            if following is None:
                return
            if following in seen:
                raise ProtocolError("Server repeated a pagination cursor")
            seen.add(following)
            cursor = following

    async def collect(self) -> list[T]:
        return [item async for item in self]
