"""Call-graph jobs and separate typed node, edge, and path iterators."""

from typing import Literal

import httpx

from .graph_models import (
    CallGraphEdge,
    CallGraphFrontier,
    CallGraphNode,
    CallGraphPath,
    CallGraphResult,
)
from .job_resource import JobResource
from .jobs import AnalysisJob
from .pages import Collection
from .selections import SymbolReference


class CallGraphs(JobResource[CallGraphResult, CallGraphEdge]):
    def __init__(self, http: httpx.Client) -> None:
        super().__init__(http, "callgraphs", CallGraphResult, CallGraphEdge)

    def create(
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
    ) -> AnalysisJob[CallGraphResult]:
        return self._create(
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

    def nodes(self, identifier: str, *, limit: int = 50) -> Collection[CallGraphNode]:
        return self._results(identifier, CallGraphNode, limit, "nodes")

    def paths(self, identifier: str, *, limit: int = 50) -> Collection[CallGraphPath]:
        return self._results(identifier, CallGraphPath, limit, "paths")

    def edges(self, identifier: str, *, limit: int = 50) -> Collection[CallGraphEdge]:
        return self.results(identifier, limit=limit)

    def frontier(
        self, identifier: str, *, limit: int = 50
    ) -> Collection[CallGraphFrontier]:
        return self._results(identifier, CallGraphFrontier, limit, "frontier")
