import sqlite3
from typing import TYPE_CHECKING, Any

from .callgraph_reader import CallGraphReader
from .evidence import EvidenceQuery
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
        self.evidence = EvidenceQuery(executor)
        self.callgraphs = CallGraphReader(facts, project, provenance)
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

    def expressions(self, *args: Any, **kwargs: Any) -> Any:
        return self.evidence.expression_occurrences(*args, **kwargs)

    def expression_occurrences(self, *args: Any, **kwargs: Any) -> Any:
        return self.evidence.expression_occurrences(*args, **kwargs)

    def field_accesses(self, *args: Any, **kwargs: Any) -> Any:
        return self.evidence.field_accesses(*args, **kwargs)

    def field_writers(self, *args: Any, **kwargs: Any) -> Any:
        return self.evidence.field_writers(*args, **kwargs)

    def field_writes(self, *args: Any, **kwargs: Any) -> Any:
        return self.evidence.field_writers(*args, **kwargs)

    def source_regions(self, *args: Any, **kwargs: Any) -> Any:
        return self.evidence.source_regions(*args, **kwargs)

    def source_sections(self, *args: Any, **kwargs: Any) -> Any:
        return self.evidence.source_regions(*args, **kwargs)

    def definition_regions(self, *args: Any, **kwargs: Any) -> Any:
        return self.evidence.source_regions(*args, **kwargs)

    def ancestors(self, ref: str, max_depth: int = 1) -> list[Any]:
        return self.graph.bases(ref, max_depth)
