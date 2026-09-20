import sqlite3
from collections.abc import Callable, Iterator

from .errors import fail
from .paths import FileResolver
from .rows import Row
from .view_details import (
    iter_definitions,
    iter_enumerations,
    iter_enumerators,
    iter_initializers,
    iter_return_types,
)
from .view_edges import iter_edges, iter_sites
from .view_evidence import iter_expression_occurrences, iter_source_regions
from .view_parameters import (
    iter_parameters,
    iter_template_arguments,
    iter_template_parameters,
)
from .view_project import iter_project
from .view_symbols import iter_symbols


class ViewLoader:
    def __init__(self, facts: sqlite3.Connection, project: sqlite3.Connection):
        self.facts, self.project = facts, project
        self.files = FileResolver(project)

    def load(self, view: str) -> list[Row]:
        return list(self.iter(view))

    def iter(self, view: str) -> Iterator[Row]:
        if (
            view in {"expression_occurrence", "source_region"}
            and self.facts.execute("PRAGMA user_version").fetchone()[0] < 13
        ):
            fail("E_CAPABILITY", f"{view} requires facts schema 13")
        functions: dict[str, Callable[[], Iterator[Row]]] = {
            "symbol": lambda: iter_symbols(self.facts, self.files),
            "parameter": lambda: iter_parameters(self.facts, self.files),
            "template_parameter": lambda: iter_template_parameters(self.facts),
            "template_argument": lambda: iter_template_arguments(self.facts),
            "edge": lambda: iter_edges(self.facts),
            "site": lambda: iter_sites(self.facts, self.files),
            "definition": lambda: iter_definitions(self.facts, self.files),
            "enumeration": lambda: iter_enumerations(self.facts),
            "enumerator": lambda: iter_enumerators(self.facts),
            "initializer": lambda: iter_initializers(self.facts),
            "return_type": lambda: iter_return_types(self.facts),
            "expression_occurrence": lambda: iter_expression_occurrences(
                self.facts, self.files
            ),
            "source_region": lambda: iter_source_regions(self.facts, self.files),
        }
        if view in {"repository", "clone", "component", "directory", "file"}:
            return iter_project(self.project, view, self.files)
        if view not in functions:
            fail("E_VIEW", f"unsupported view {view!r}")
        return functions[view]()
