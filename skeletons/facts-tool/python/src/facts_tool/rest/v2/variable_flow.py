"""Tracking requests identify a function and a scoped local declaration."""

from typing import Literal

import httpx

from .flow_models import FlowBoundary, FlowEdge, FlowNode, VariableFlowResult
from .job_resource import JobResource
from .jobs import AnalysisJob
from .pages import Collection
from .selections import Selection, SymbolReference, VariableReference


class VariableFlow(JobResource[VariableFlowResult, FlowEdge]):
    def __init__(self, http: httpx.Client) -> None:
        super().__init__(http, "variable-flow", VariableFlowResult, FlowEdge)

    def create(
        self,
        *,
        function: SymbolReference,
        variable: VariableReference,
        selection: Selection | None = None,
        direction: Literal["forward"] = "forward",
        interprocedural: bool = True,
        max_call_depth: int | None = None,
    ) -> AnalysisJob[VariableFlowResult]:
        return self._create(
            {
                "function": function,
                "variable": variable,
                "selection": selection,
                "direction": direction,
                "interprocedural": interprocedural,
                "max_call_depth": max_call_depth,
            }
        )

    def nodes(self, identifier: str, *, limit: int = 50) -> Collection[FlowNode]:
        return self._results(identifier, FlowNode, limit, "nodes")

    def boundaries(
        self, identifier: str, *, limit: int = 50
    ) -> Collection[FlowBoundary]:
        return self._results(identifier, FlowBoundary, limit, "boundaries")

    def edges(self, identifier: str, *, limit: int = 50) -> Collection[FlowEdge]:
        return self.results(identifier, limit=limit)
