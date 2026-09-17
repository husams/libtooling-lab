"""Observable cache telemetry and semantic database checks."""
import sqlite3

from support.database import query

FACT_TABLES = ("symbol", "definition", "callable_return_type", "parameter",
               "variable_initializer", "template_argument", "template_parameter",
               "relation", "relation_site", "include_dependency")


def fact_snapshot(project):
    with sqlite3.connect(project.facts) as connection:
        return {table: sorted(connection.execute(f'SELECT * FROM "{table}"').fetchall(),
                              key=repr) for table in FACT_TABLES}


def require_hit(project):
    project.succeed()
    assert "ast-cache: hit" in project.last.stderr, project.last.stderr
    assert "ast-cache: miss" not in project.last.stderr, project.last.stderr


def require_miss(project):
    project.succeed()
    assert "ast-cache: miss" in project.last.stderr, project.last.stderr
    assert "ast-cache: hit" not in project.last.stderr, project.last.stderr


def require_stored(project):
    project.succeed()
    assert "ast-cache: stored" in project.last.stderr, project.last.stderr
    files = project.ast_files()
    assert files, f"No serialized AST in {project.cache}: {project.last.stderr}"
    assert all(path.stat().st_size > 100 for path in files)
    assert not list(project.cache.rglob("*.json")), "AST cache wrote legacy JSON metadata"


def require_symbol(project, name):
    rows = query(project.facts, "SELECT qualified_name FROM symbol WHERE qualified_name=?", (name,))
    assert rows, f"Missing fresh symbol {name}: {project.last.stderr}"


def require_consumer_result(project, family):
    if family in ("extract", "match"):
        require_symbol(project, "cache_root")
    elif family == "import":
        assert query(project.conf, "SELECT name FROM file WHERE name='cache.hpp'")
    elif family == "dependency":
        assert query(project.facts, "SELECT * FROM include_dependency")
    elif family == "variable-flow":
        flow = project.root / "flow.sqlite"
        assert query(flow, "SELECT status FROM variable_flow_run ORDER BY run_id DESC LIMIT 1") == [("complete",)]
        assert query(flow, "SELECT * FROM variable_flow_node")


def require_dependency_hit(project):
    project.succeed()
    assert "dependency-cache: hit" in project.last.stderr, project.last.stderr
    assert "dependency-cache: miss" not in project.last.stderr, project.last.stderr
    if project.last_family == "import":
        assert "ast-cache: miss" not in project.last.stderr, project.last.stderr
        assert "ast-cache: stored" not in project.last.stderr, project.last.stderr
    else:
        assert "ast-cache:" not in project.last.stderr, project.last.stderr


def require_consumer_cache_hit(project):
    if project.last_family in ("import", "dependency"):
        require_dependency_hit(project)
    else:
        require_hit(project)
