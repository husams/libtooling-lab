from dataclasses import dataclass
from typing import Any

from .callgraph_frontier import CallGraphFrontier
from .callgraph_models import (
    CallGraphEdge,
    CallGraphRoot,
    CallGraphTarget,
)
from .callgraph_page import CallGraphPage
from .callgraph_recovery import CallGraphRecovery
from .provenance import PairProvenance


@dataclass(frozen=True)
class CallGraphRun:
    run_id: int
    created_at: str
    project_path: str
    facts_path: str
    mode: str
    path_mode: str | None
    calls_scope: str
    components: tuple[str, ...]
    max_depth: int | None
    max_nodes: int | None
    max_edges: int | None
    time_limit_ms: int | None
    recover_missing: bool
    status: str
    truncation_reason: str | None
    error: str | None
    roots: CallGraphPage[CallGraphRoot]
    targets: CallGraphPage[CallGraphTarget]
    edges: CallGraphPage[CallGraphEdge]
    frontier: CallGraphPage[CallGraphFrontier]
    recovery: CallGraphPage[CallGraphRecovery]
    provenance: PairProvenance
    target_exists: bool = False
    target_was_reached: bool = False
    self_path_exists: bool = False

    @property
    def target(self) -> CallGraphTarget | None:
        return self.targets[0] if self.targets.items else None

    @property
    def sites(self) -> tuple[Any, ...]:
        return tuple(edge.site for edge in self.edges if edge.site is not None)

    @property
    def target_reached(self) -> bool:
        return self.target_was_reached

    @property
    def self_path(self) -> bool:
        return self.self_path_exists

    @property
    def path_found(self) -> bool:
        return self.path_outcome == "found"

    @property
    def path_outcome(self) -> str:
        if not self.target_exists:
            return "not-applicable"
        if self.target_reached or self.self_path:
            return "found"
        return "unreachable" if self.status == "complete" else "truncated"

    @property
    def truncated(self) -> bool:
        return any(
            page.truncated
            for page in (
                self.roots,
                self.targets,
                self.edges,
                self.frontier,
                self.recovery,
            )
        )

    @property
    def boundaries(self) -> CallGraphPage[CallGraphFrontier]:
        return self.frontier

    def to_dict(self) -> dict[str, Any]:
        value = self.__dict__.copy()
        for name in ("roots", "targets", "edges", "frontier", "recovery"):
            page = value[name]
            value[name] = {
                "items": [row.to_dict() for row in page.items],
                "total": page.total,
                "next_cursor": page.next_cursor,
                "complete": page.complete,
            }
        value["components"] = list(self.components)
        value["provenance"] = self.provenance.to_dict()
        return value
