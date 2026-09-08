from collections.abc import Callable
from typing import Any


def assert_facade_matrix(cb: Any) -> None:
    target = "app::Box::value"
    ref = "app::Box"
    result_calls: list[tuple[str, Callable[[], Any]]] = [
        ("codebase.expressions", cb.expressions),
        ("codebase.expression_occurrences", cb.expression_occurrences),
        ("codebase.field_accesses", lambda: cb.field_accesses(target)),
        ("codebase.field_writers", lambda: cb.field_writers(target)),
        ("codebase.field_writes", lambda: cb.field_writes(target)),
        ("codebase.source_regions", cb.source_regions),
        ("codebase.source_sections", cb.source_sections),
        ("codebase.definition_regions", cb.definition_regions),
        ("evidence.expression_occurrences", cb.evidence.expression_occurrences),
        ("evidence.expressions", cb.evidence.expressions),
        ("evidence.field_accesses", lambda: cb.evidence.field_accesses(target)),
        ("evidence.field_writers", lambda: cb.evidence.field_writers(target)),
        ("evidence.field_writes", lambda: cb.evidence.field_writes(target)),
        ("evidence.source_regions", cb.evidence.source_regions),
        ("evidence.source_sections", cb.evidence.source_sections),
        ("evidence.definition_regions", cb.evidence.definition_regions),
        ("graph.expression_occurrences", cb.graph.expression_occurrences),
        ("graph.expressions", cb.graph.expressions),
        ("graph.field_accesses", lambda: cb.graph.field_accesses(target)),
        ("graph.field_writers", lambda: cb.graph.field_writers(target)),
        ("graph.field_writes", lambda: cb.graph.field_writes(target)),
        ("graph.source_regions", cb.graph.source_regions),
        ("graph.source_sections", cb.graph.source_sections),
        ("graph.definition_regions", cb.graph.definition_regions),
    ]
    for name, call in result_calls:
        result = call()
        assert result.rows, name
    entity_calls: list[tuple[str, Callable[[], Any]]] = [
        ("codebase.ancestors", lambda: cb.ancestors(ref)),
        ("evidence.ancestors", lambda: cb.evidence.ancestors(ref)),
        ("graph.ancestors", lambda: cb.graph.ancestors(ref)),
    ]
    for name, call in entity_calls:
        result = call()
        assert [item.qualified_name for item in result] == ["app::Base"], name
