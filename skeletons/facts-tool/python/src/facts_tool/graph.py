from typing import TYPE_CHECKING

from .entity import Entity, make_entity
from .errors import FactsToolError
from .executor import Executor
from .queryplan.sources import codebase, start, symbol
from .queryplan.stages import in_, nodes, out, sites, view
from .rows import Row

if TYPE_CHECKING:
    from .fluent import EntityQuery


class GraphQuery:
    def __init__(self, executor: Executor):
        self.executor = executor

    def get(self, ref: str) -> Entity:
        result = self.executor.run(start(symbol(ref)).plan)
        return make_entity(result.nodes[0], self)

    def find(self, ref: str) -> Entity | None:
        try:
            return self.get(ref)
        except FactsToolError as exc:
            if exc.code == "E_SOURCE" and "not found" in exc.message:
                return None
            raise

    def query(self, ref: str | None = None) -> "EntityQuery":
        from .fluent import EntityQuery

        source = symbol(ref) if ref is not None else codebase()
        return EntityQuery(self.executor, start(source))

    def neighbors(
        self,
        ref: str,
        relation: str,
        *,
        inbound: bool = False,
        min_depth: int = 1,
        max_depth: int = 1,
    ) -> list[Entity]:
        stage = (
            in_(relation, min_depth, max_depth)
            if inbound
            else out(relation, min_depth, max_depth)
        )
        rows = self.executor.run((start(symbol(ref)) | stage).plan).nodes
        return [make_entity(row, self) for row in rows]

    def reaches(
        self, source: str, target: str, relation: str, max_depth: int = 8
    ) -> bool:
        return any(
            row.usr == target or row.qualified_name == target
            for row in self.neighbors(source, relation, max_depth=max_depth)
        )

    def callers(self, ref: str, max_depth: int = 1) -> list[Entity]:
        return self.neighbors(ref, "calls", inbound=True, max_depth=max_depth)

    def callees(self, ref: str, max_depth: int = 1) -> list[Entity]:
        return self.neighbors(ref, "calls", max_depth=max_depth)

    def bases(self, ref: str, max_depth: int = 1) -> list[Entity]:
        return self.neighbors(ref, "inherits", max_depth=max_depth)

    def subclasses(self, ref: str, max_depth: int = 1) -> list[Entity]:
        return self.neighbors(ref, "inherits", inbound=True, max_depth=max_depth)

    def members(self, ref: str) -> list[Entity]:
        return self.neighbors(ref, "contains")

    def parameters(self, ref: str) -> list[Row]:
        return list(self.executor.run((start(symbol(ref)) | out("has_parameter")).plan))

    def definitions(self, ref: str) -> list[Row]:
        return list(self.executor.run((start(symbol(ref)) | out("definition")).plan))

    def references(self, ref: str) -> list[Row]:
        target = self.get(ref).id
        query = start(codebase()) | view("edge") | nodes() | sites()
        return [
            row
            for row in self.executor.run(query.plan)
            if row["destination_id"] == target
        ]
