from collections.abc import Iterable
from typing import TYPE_CHECKING

from .errors import fail
from .variableflow_models import Boundary, Edge, Node
from .variableflow_query_helpers import edges_for
from .variableflow_query_helpers import node_id as resolve_node_id

if TYPE_CHECKING:
    from .variableflow_models import VariableFlowRun


_READ_KINDS = frozenset(("read", "update"))
_WRITE_KINDS = frozenset(("write", "update"))


class VariableFlowGraph:
    """Indexed, immutable queries for one :class:`VariableFlowRun`."""

    def __init__(self, run: "VariableFlowRun") -> None:
        self._nodes = tuple(run.nodes)
        self._edges = tuple(run.edges)
        self._boundaries = tuple(run.boundaries)
        self._by_id = {node.id: node for node in self._nodes}

    def nodes(
        self,
        kind: str | Iterable[str] | None = None,
        *,
        name: str | None = None,
        function_usr: str | None = None,
        variable_usr: str | None = None,
        node_id: int | None = None,
    ) -> tuple[Node, ...]:
        kinds = {kind} if isinstance(kind, str) else set(kind or ())
        return tuple(
            node
            for node in self._nodes
            if (not kinds or node.kind in kinds)
            and (name is None or node.name == name)
            and (function_usr is None or node.function_usr == function_usr)
            and (variable_usr is None or node.variable_usr == variable_usr)
            and (node_id is None or node.id == node_id)
        )

    def node(self, node_id: int) -> Node:
        """Resolve one node ID for inspecting edge endpoints and callsites."""
        if not isinstance(node_id, int) or isinstance(node_id, bool):
            fail("E_SOURCE", "node_id must be an integer")
        try:
            return self._by_id[node_id]
        except KeyError:
            fail("E_SOURCE", f"variable-flow node {node_id} not found")

    def reads(
        self,
        *,
        variable_usr: str | None = None,
        function_usr: str | None = None,
    ) -> tuple[Node, ...]:
        return self.nodes(
            _READ_KINDS, variable_usr=variable_usr, function_usr=function_usr
        )

    def writes(
        self,
        *,
        variable_usr: str | None = None,
        function_usr: str | None = None,
    ) -> tuple[Node, ...]:
        return self.nodes(
            _WRITE_KINDS, variable_usr=variable_usr, function_usr=function_usr
        )

    def incoming(
        self, node: Node | int, *, kind: str | None = None
    ) -> tuple[Edge, ...]:
        return edges_for(self._edges, node, kind, "target", self._by_id)

    def outgoing(
        self, node: Node | int, *, kind: str | None = None
    ) -> tuple[Edge, ...]:
        return edges_for(self._edges, node, kind, "source", self._by_id)

    def boundaries(
        self, node: Node | int | None = None, *, reason: str | None = None
    ) -> tuple[Boundary, ...]:
        node_id = None if node is None else resolve_node_id(node, self._by_id)
        return tuple(
            boundary
            for boundary in self._boundaries
            if (node_id is None or boundary.node == node_id)
            and (reason is None or boundary.reason == reason)
        )
