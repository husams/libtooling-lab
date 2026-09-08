from typing import TYPE_CHECKING, Any

from .evidence import EvidenceQuery

if TYPE_CHECKING:
    from .executor import Executor


class GraphEvidenceMethods:
    executor: "Executor"

    def expression_occurrences(self, *args: Any, **kwargs: Any) -> Any:
        return EvidenceQuery(self.executor).expression_occurrences(*args, **kwargs)

    def expressions(self, *args: Any, **kwargs: Any) -> Any:
        return self.expression_occurrences(*args, **kwargs)

    def field_accesses(self, *args: Any, **kwargs: Any) -> Any:
        return EvidenceQuery(self.executor).field_accesses(*args, **kwargs)

    def field_writers(self, *args: Any, **kwargs: Any) -> Any:
        return EvidenceQuery(self.executor).field_writers(*args, **kwargs)

    def field_writes(self, *args: Any, **kwargs: Any) -> Any:
        return self.field_writers(*args, **kwargs)

    def source_regions(self, *args: Any, **kwargs: Any) -> Any:
        return EvidenceQuery(self.executor).source_regions(*args, **kwargs)

    def source_sections(self, *args: Any, **kwargs: Any) -> Any:
        return self.source_regions(*args, **kwargs)

    def definition_regions(self, *args: Any, **kwargs: Any) -> Any:
        return self.source_regions(*args, **kwargs)
