"""Call-graph jobs and separate typed node, edge, and path iterators."""

from typing import Literal

import httpx

from .async_job_resource import JobResource
from .async_jobs import AsyncAnalysisJob
from .graph_models import (
    CallGraphEdge,
    CallGraphFrontier,
    CallGraphNode,
    CallGraphPath,
    CallGraphResult,
)
from .pages import AsyncCollection
from .selections import SymbolReference


class CallGraphs(JobResource[CallGraphResult, CallGraphEdge]):
    def __init__(self, http: httpx.AsyncClient) -> None:
        super().__init__(http, "callgraphs", CallGraphResult, CallGraphEdge)

    async def create(
        self,
        *,
        root: SymbolReference,
        direction: Literal["callees", "callers"] = "callees",
        max_depth: int | None = None,
        max_nodes: int | None = None,
        max_edges: int | None = None,
        time_limit_ms: int | None = None,
        target: SymbolReference | None = None,
        path_mode: Literal["shortest", "all_simple"] | None = None,
    ) -> AsyncAnalysisJob[CallGraphResult]:
        return await self._create(
            {
                "root": root,
                "direction": direction,
                "max_depth": max_depth,
                "max_nodes": max_nodes,
                "max_edges": max_edges,
                "time_limit_ms": time_limit_ms,
                "target": target,
                "path_mode": path_mode,
            }
        )

    def nodes(
        self, identifier: str, *, limit: int = 50
    ) -> AsyncCollection[CallGraphNode]:
        return self._results(identifier, CallGraphNode, limit, "nodes")

    def paths(
        self, identifier: str, *, limit: int = 50
    ) -> AsyncCollection[CallGraphPath]:
        return self._results(identifier, CallGraphPath, limit, "paths")

    def edges(
        self, identifier: str, *, limit: int = 50
    ) -> AsyncCollection[CallGraphEdge]:
        return self.results(identifier, limit=limit)

    def frontier(
        self, identifier: str, *, limit: int = 50
    ) -> AsyncCollection[CallGraphFrontier]:
        return self._results(identifier, CallGraphFrontier, limit, "frontier")
