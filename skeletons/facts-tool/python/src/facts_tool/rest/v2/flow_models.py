"""Interprocedural variable tracking with explicit uncertainty boundaries."""

from dataclasses import dataclass
from typing import Literal

from .analysis_models import Diagnostic
from .match_models import MatchLocation


@dataclass(frozen=True, slots=True)
class FlowNode:
    id: int
    kind: str
    function_usr: str
    variable_usr: str
    name: str
    type: str
    location: MatchLocation
    block: int
    depth: int


@dataclass(frozen=True, slots=True)
class FlowEdge:
    source: int
    target: int
    kind: str
    callsite: int


@dataclass(frozen=True, slots=True)
class FlowBoundary:
    node: int
    reason: str
    detail: str
    depth: int


@dataclass(frozen=True, slots=True)
class VariableFlowResult:
    root_function: str
    root_variable: str
    status: str
    coverage: Literal["complete", "partial"]
    node_count: int
    edge_count: int
    nodes: tuple[FlowNode, ...] | None = None
    edges: tuple[FlowEdge, ...] | None = None
    boundaries: tuple[FlowBoundary, ...] | None = None
    diagnostics: tuple[Diagnostic, ...] | None = None
