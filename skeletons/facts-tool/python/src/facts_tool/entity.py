from typing import TYPE_CHECKING, Any

from .rows import Row, public_row


class Entity:
    def __init__(self, row: Row, graph: "GraphQuery"):
        self._row, self._graph = row, graph

    def __getattr__(self, name: str) -> Any:
        try:
            return self._row[name]
        except KeyError as exc:
            raise AttributeError(name) from exc

    def to_dict(self) -> Row:
        return public_row(self._row)

    def outgoing(self, relation: str, max_depth: int = 1) -> list["Entity"]:
        return self._graph.neighbors(self.usr, relation, max_depth=max_depth)

    def incoming(self, relation: str, max_depth: int = 1) -> list["Entity"]:
        return self._graph.neighbors(
            self.usr, relation, inbound=True, max_depth=max_depth
        )

    def definitions(self) -> list[Row]:
        return self._graph.definitions(self.usr)

    def references(self) -> list[Row]:
        return self._graph.references(self.usr)


class Callable(Entity):
    def callers(self, max_depth: int = 1) -> list[Entity]:
        return self.incoming("calls", max_depth)

    def callees(self, max_depth: int = 1) -> list[Entity]:
        return self.outgoing("calls", max_depth)

    def parameters(self) -> list[Row]:
        return self._graph.parameters(self.usr)


class Method(Callable):
    def record(self) -> Entity | None:
        rows = self.outgoing("method_of")
        return rows[0] if rows else None


class Record(Entity):
    def bases(self, max_depth: int = 1) -> list[Entity]:
        return self.outgoing("inherits", max_depth)

    def subclasses(self, max_depth: int = 1) -> list[Entity]:
        return self.incoming("inherits", max_depth)

    def methods(self) -> list[Entity]:
        return self.incoming("method_of")

    def fields(self) -> list[Entity]:
        return self.incoming("field_of")


def make_entity(row: Row, graph: "GraphQuery") -> Entity:
    if row.get("node_kind") == "record":
        return Record(row, graph)
    if row.get("node_kind") == "function":
        kind = str(row.get("kind", ""))
        return Method(row, graph) if "method" in kind else Callable(row, graph)
    return Entity(row, graph)


if TYPE_CHECKING:
    from .graph import GraphQuery
