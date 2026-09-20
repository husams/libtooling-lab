"""Call-graph nodes, call sites, traversal boundaries, and paths."""

from dataclasses import dataclass
from typing import Literal

from .analysis_models import Diagnostic


@dataclass(frozen=True, slots=True)
class CallGraphNode:
    symbol_id: str
    qualified_name: str
    usr: str
    definition: bool
    external: bool
    file_id: str
    line: int
    column: int
    unresolved_calls: int
    pointer_calls: int


@dataclass(frozen=True, slots=True)
class CallGraphEdge:
    source: str
    target: str
    kind: Literal["call", "dispatch_call"]
    file_id: str
    line: int
    column: int
    offset: int
    implicit: bool
    depth: int
    cycle: bool
    external_boundary: bool
    definition_boundary: bool


@dataclass(frozen=True, slots=True)
class CallGraphPath:
    nodes: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class CallGraphFrontier:
    symbol_id: str
    reason: str


@dataclass(frozen=True, slots=True)
class CallGraphResult:
    coverage: Literal["complete", "partial"]
    truncated: bool
    truncation_reason: str | None
    node_count: int
    edge_count: int
    nodes: tuple[CallGraphNode, ...] | None = None
    edges: tuple[CallGraphEdge, ...] | None = None
    paths: tuple[CallGraphPath, ...] | None = None
    frontier: tuple[CallGraphFrontier, ...] | None = None
    diagnostics: tuple[Diagnostic, ...] | None = None
