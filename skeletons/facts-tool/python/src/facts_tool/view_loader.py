import sqlite3

from .errors import fail
from .paths import FileResolver
from .rows import Row
from .view_details import (
    load_definitions,
    load_enumerations,
    load_enumerators,
    load_initializers,
    load_return_types,
)
from .view_edges import load_edges, load_sites
from .view_parameters import (
    load_parameters,
    load_template_arguments,
    load_template_parameters,
)
from .view_project import load_project
from .view_symbols import load_symbols


class ViewLoader:
    def __init__(self, facts: sqlite3.Connection, project: sqlite3.Connection):
        self.facts, self.project = facts, project
        self.files = FileResolver(project)

    def load(self, view: str) -> list[Row]:
        functions = {
            "symbol": lambda: load_symbols(self.facts, self.files),
            "parameter": lambda: load_parameters(self.facts, self.files),
            "template_parameter": lambda: load_template_parameters(self.facts),
            "template_argument": lambda: load_template_arguments(self.facts),
            "edge": lambda: load_edges(self.facts),
            "site": lambda: load_sites(self.facts, self.files),
            "definition": lambda: load_definitions(self.facts, self.files),
            "enumeration": lambda: load_enumerations(self.facts),
            "enumerator": lambda: load_enumerators(self.facts),
            "initializer": lambda: load_initializers(self.facts),
            "return_type": lambda: load_return_types(self.facts),
        }
        if view in {"repository", "clone", "component", "directory", "file"}:
            return load_project(self.project, view, self.files)
        if view not in functions:
            fail("E_VIEW", f"unsupported view {view!r}")
        return functions[view]()
