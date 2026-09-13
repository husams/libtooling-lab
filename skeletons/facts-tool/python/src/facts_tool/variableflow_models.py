from dataclasses import asdict, dataclass
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from .variableflow_query import VariableFlowGraph


@dataclass(frozen=True)
class Location:
    file: str
    line: int
    column: int
    offset: int


@dataclass(frozen=True)
class Node:
    id: int
    kind: str
    function_usr: str
    variable_usr: str
    name: str
    type: str
    location: Location
    block: int = -1
    depth: int = 0


@dataclass(frozen=True)
class Edge:
    source: int
    target: int
    kind: str
    callsite: int = 0


@dataclass(frozen=True)
class Boundary:
    node: int
    reason: str
    detail: str
    depth: int = 0


@dataclass(frozen=True)
class VariableFlowRun:
    run_id: int
    created_at: str
    project_path: str
    facts_path: str
    function_selector: str
    variable_selector: str
    sources: tuple[str, ...]
    declaration_line: int | None
    max_depth: int | None
    engine: str
    assumptions: str
    root_function: str
    root_variable: str
    status: str
    nodes: tuple[Node, ...]
    edges: tuple[Edge, ...]
    boundaries: tuple[Boundary, ...]

    @property
    def function(self) -> str:
        return self.function_selector

    @property
    def variable(self) -> str:
        return self.variable_selector

    @property
    def graph(self) -> "VariableFlowGraph":
        from .variableflow_query import VariableFlowGraph

        return VariableFlowGraph(self)

    @property
    def line(self) -> int | None:
        return self.declaration_line

    def to_dict(self) -> dict[str, Any]:
        value = asdict(self)
        value["sources"] = list(self.sources)
        return value
