"""Symbol name lookup defaults to a literal, case-sensitive prefix."""

from typing import Literal, TypeVar

import httpx

from .async_resource import Resource
from .codec import decode
from .pages import AsyncCollection, validate_limit
from .symbol_models import Symbol, SymbolOccurrence, SymbolRelation
from .wire import async_call, path, query

T = TypeVar("T")


class Symbols(Resource[Symbol]):
    def __init__(self, http: httpx.AsyncClient) -> None:
        super().__init__(http, "symbols", Symbol)

    def find(
        self,
        qualified_name: str | None = None,
        *,
        usr: str | None = None,
        kind: str | None = None,
        match: Literal["prefix", "exact"] = "prefix",
        repository: str | None = None,
        component: str | None = None,
        limit: int = 50,
    ) -> AsyncCollection[Symbol]:
        if not qualified_name and not usr:
            raise ValueError("qualified_name or usr is required")
        if match not in {"prefix", "exact"}:
            raise ValueError("match must be prefix or exact")
        if validate_limit(limit) > 500:
            raise ValueError("symbol limit cannot exceed 500")
        return self._collection(
            {
                "qualified_name": qualified_name,
                "usr": usr,
                "kind": kind,
                "match": match,
                "repository": repository,
                "component": component,
                "limit": limit,
            }
        )

    def occurrences(
        self, identifier: str, *, limit: int = 50
    ) -> AsyncCollection[SymbolOccurrence]:
        return self._children(identifier, "occurrences", SymbolOccurrence, limit, {})

    def relations(
        self,
        identifier: str,
        *,
        kind: str | None = None,
        direction: Literal["outgoing", "incoming", "both"] = "outgoing",
        limit: int = 50,
    ) -> AsyncCollection[SymbolRelation]:
        return self._children(
            identifier,
            "relations",
            SymbolRelation,
            limit,
            {"kind": kind, "direction": direction},
        )

    def _children(
        self,
        identifier: str,
        child: str,
        model: type[T],
        limit: int,
        filters: dict[str, object],
    ) -> AsyncCollection[T]:
        if validate_limit(limit) > 500:
            raise ValueError("symbol limit cannot exceed 500")
        route = path("symbols", identifier) + "/" + child

        async def fetch(cursor: str | None) -> dict[str, object]:
            return await async_call(
                self._http,
                "GET",
                query(
                    route,
                    {
                        **filters,
                        "limit": limit,
                        "cursor": cursor,
                    },
                ),
            )

        return AsyncCollection(fetch, lambda value: decode(model, value))
