import sqlite3
from contextlib import closing

import pytest

from facts_tool import FactsToolError
from facts_tool.paths import FileResolver


def test_repeated_file_paths_query_project_once(paired_databases):
    with closing(sqlite3.connect(paired_databases[1])) as project:
        project.row_factory = sqlite3.Row
        statements = []
        project.set_trace_callback(statements.append)
        resolver = FileResolver(project)
        path = resolver.path(1)
        assert path.endswith("src/main.cpp")
        assert resolver.path(1) == path
        assert len(statements) == 1


def test_optional_missing_path_still_fails_when_required(paired_databases):
    with closing(sqlite3.connect(paired_databases[1])) as project:
        project.row_factory = sqlite3.Row
        resolver = FileResolver(project)
        assert resolver.path(999, required=False) is None
        with pytest.raises(FactsToolError, match="E_IDENTITY"):
            resolver.path(999)
        assert resolver.path(0) is None


def test_path_cache_is_bounded_and_evicts_least_recently_used(paired_databases):
    with closing(sqlite3.connect(paired_databases[1])) as project:
        project.row_factory = sqlite3.Row
        statements = []
        project.set_trace_callback(statements.append)
        resolver = FileResolver(project)
        resolver.path(1)
        resolver.path(2)
        for file_id in range(3, 4097):
            resolver.path(file_id, required=False)
        resolver.path(1)
        resolver.path(4097, required=False)
        assert len(statements) == 4097
        resolver.path(1)
        assert len(statements) == 4097
        resolver.path(2)
        assert len(statements) == 4098
