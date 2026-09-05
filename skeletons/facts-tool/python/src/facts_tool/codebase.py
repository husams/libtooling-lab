import sqlite3
from typing import TYPE_CHECKING

from .executor import Executor
from .graph import GraphQuery
from .provenance import PairProvenance

if TYPE_CHECKING:
    from .entity import Entity
    from .fluent import EntityQuery


class CodeBase:
    def __init__(
        self,
        facts: sqlite3.Connection,
        project: sqlite3.Connection,
        executor: Executor,
        provenance: PairProvenance,
    ):
        self._facts, self._project = facts, project
        self.executor, self.provenance = executor, provenance
        self.graph = GraphQuery(executor)
        self._closed = False

    def __enter__(self) -> "CodeBase":
        return self

    def __exit__(self, *_args: object) -> None:
        self.close()

    def close(self) -> None:
        if not self._closed:
            try:
                self._facts.close()
            finally:
                self._project.close()
                self._closed = True

    def get(self, ref: str) -> "Entity":
        return self.graph.get(ref)

    def find(self, ref: str) -> "Entity | None":
        return self.graph.find(ref)

    def query(self, ref: str | None = None) -> "EntityQuery":
        return self.graph.query(ref)
