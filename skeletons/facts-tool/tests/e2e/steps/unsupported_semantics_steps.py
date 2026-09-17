"""Coverage regressions exercise persisted call sites and cached AST replay."""
import os
import sqlite3

from pytest_bdd import given, parsers, then, when

from support.ast_cache import AstCacheProject
from support.ast_cache_assertions import fact_snapshot, require_hit
from support.ast_cache_git import initialize_repository


@given(parsers.parse('the semantic coverage fixture "{name}"'),
       target_fixture="semantic_coverage")
def coverage_fixture(context, tmp_path, name):
    root = tmp_path / name
    root.mkdir()
    source = context.fixture_root / "unsupported_semantics" / f"{name}.cpp"
    (root / "cache.cpp").write_text(source.read_text(encoding="utf-8"),
                                   encoding="utf-8")
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(("FACTS_TOOL_", "XDG_", "GIT_"))}
    environment["XDG_CONFIG_HOME"] = str(root / "user-config")
    environment["XDG_DATA_HOME"] = str(root / "user-data")
    project = AstCacheProject(context.facts_tool, context.compiler, root,
                              environment, root / ".facts-tool" / "ast-cache")
    initialize_repository(root, environment)
    project.write_commands()
    project.configure(ast_cache=True)
    project.run("import")
    project.succeed()
    return project


@when("the semantic coverage fixture is extracted at verbosity zero")
def extract(semantic_coverage):
    project = semantic_coverage
    command = project.command("extract")
    command[command.index("-v") + 1] = "0"
    project.run("extract", command=command)
    project.succeed()
    assert "symbol(s) recorded" in project.last.stderr, project.last.stderr
    project.baseline = semantic_snapshot(project)
    project.coverage_diagnostics = diagnostics(project)


def diagnostics(project):
    return sorted(line for line in project.last.stderr.splitlines()
                  if "coverage.unsupported_semantics" in line)


def unresolved_sites(project):
    with sqlite3.connect(project.facts) as database:
        return database.execute(
            "SELECT source.qualified_name,site.file_id,site.offset,site.line,site.col "
            "FROM callgraph_unresolved_site AS site "
            "JOIN symbol AS source ON source.id=site.source_id ORDER BY 1,2,3"
        ).fetchall()


def semantic_snapshot(project):
    return {**fact_snapshot(project),
            "callgraph_unresolved_site": unresolved_sites(project)}


def call_sites(project):
    with sqlite3.connect(project.facts) as database:
        return database.execute(
            "SELECT source.qualified_name,target.qualified_name,site.line,"
            "site.certainty,relation.is_implicit FROM relation_site AS site "
            "JOIN symbol AS source ON source.id=site.source_id "
            "JOIN symbol AS target ON target.id=site.destination_id "
            "JOIN relation ON relation.source_id=site.source_id "
            "AND relation.destination_id=site.destination_id "
            "AND relation.kind=site.kind AND relation.position=site.position "
            "WHERE site.kind=1 ORDER BY 1,2,3"
        ).fetchall()


@then("initialized local function pointers have exact call edges")
def exact_local_edges(semantic_coverage):
    rows = call_sites(semantic_coverage)
    expected = {("caller", "target", 5, None, 0),
                ("address_target", "target", 10, None, 0),
                ("parenthesized_target", "target", 15, None, 0)}
    assert set(rows) == expected, rows
    assert not unresolved_sites(semantic_coverage)


@then("uncertain function pointers have diagnostics without guessed targets")
def uncertain_edges(semantic_coverage):
    project = semantic_coverage
    lines = [number for number, line in enumerate(
        project.source.read_text(encoding="utf-8").splitlines(), start=1)
        if "// unresolved" in line]
    expected = sorted(
        "facts-tool: coverage.unsupported_semantics kind=indirect-call "
        f"site={project.source}:{number}:5" for number in lines)
    assert diagnostics(project) == expected, project.last.stderr
    callers = {"parameter", "reassigned", "escaped", "aliased", "captured"}
    unresolved = unresolved_sites(project)
    assert len(unresolved) == len(callers), unresolved
    assert {row[0] for row in unresolved} == callers, unresolved
    assert {row[3] for row in unresolved} == set(lines), unresolved
    guessed = [row for row in call_sites(project)
               if row[0] in callers and row[1] in {"target", "other"}]
    assert not guessed, guessed


@then("vector and unique pointer cleanup edges are preserved")
def standard_library_cleanup(semantic_coverage):
    rows = call_sites(semantic_coverage)
    assert any(source == "caller" and "vector" in target and "::~vector" in target
               and implicit for source, target, _, _, implicit in rows), rows
    assert any(source == "caller" and "unique_ptr" in target
               and "::~unique_ptr" in target and implicit
               for source, target, _, _, implicit in rows), rows
    with sqlite3.connect(semantic_coverage.facts) as database:
        assert database.execute(
            "SELECT 1 FROM symbol WHERE qualified_name='Widget::~Widget'"
        ).fetchall()


@then("defaulted cleanup retains its nontrivial destructor edges")
def defaulted_cleanup(semantic_coverage):
    rows = call_sites(semantic_coverage)
    implicit_edges = {(source, target) for source, target, _, _, implicit in rows
                      if implicit}
    expected = {("caller", "Owner::~Owner"),
                ("temporary", "Owner::~Owner"),
                ("Owner::~Owner", "Base::~Base"),
                ("Owner::~Owner", "Member::~Member"),
                ("Owner::~Owner", "ArrayElement::~ArrayElement")}
    assert expected <= implicit_edges, rows


@then("extraction emits no unsupported semantics diagnostics")
def no_unsupported(semantic_coverage):
    assert not diagnostics(semantic_coverage), semantic_coverage.last.stderr


@then("repeated cached extraction preserves semantic facts and diagnostics")
def cached_repetition(semantic_coverage):
    project = semantic_coverage
    for _ in range(2):
        project.run("extract")
        require_hit(project)
        assert "dependency-cache: hit" in project.last.stderr, project.last.stderr
        assert semantic_snapshot(project) == project.baseline
        assert diagnostics(project) == project.coverage_diagnostics
