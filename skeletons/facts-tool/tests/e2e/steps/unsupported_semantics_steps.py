"""Coverage regressions exercise persisted call sites and cached AST replay."""
import json
import os
import sqlite3

from pytest_bdd import given, parsers, then, when

from support.ast_cache import AstCacheProject
from support.ast_cache_assertions import fact_snapshot, require_hit
from support.ast_cache_git import initialize_repository
from support.callgraph_run import completion


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
            "callgraph_unresolved_site": unresolved_sites(project),
            "callgraph_pointer_call_site": pointer_sites(project)}


def pointer_sites(project):
    with sqlite3.connect(project.facts) as database:
        database.row_factory = sqlite3.Row
        return [dict(row) for row in database.execute(
            "SELECT site.*,source.qualified_name AS source,"
            "target.qualified_name AS target,target.usr AS target_usr,"
            "target.is_external AS target_external FROM callgraph_pointer_call_site AS site "
            "JOIN symbol AS source ON source.id=site.source_id "
            "LEFT JOIN symbol AS target ON target.id=site.target_id "
            "ORDER BY site.source_id,site.file_id,site.offset")]


def assert_pointer_relations(project, pointers):
    expected = {(row["source_id"], row["target_id"], row["file_id"], row["offset"])
                for row in pointers if row["target_id"] is not None}
    with sqlite3.connect(project.facts) as database:
        actual = set(database.execute(
            "SELECT source_id,destination_id,file_id,offset FROM relation_site "
            "WHERE kind=24"))
        relations = set(database.execute(
            "SELECT source_id,destination_id FROM relation WHERE kind=24"))
    assert actual == expected, (actual, expected)
    assert relations == {(row[0], row[1]) for row in expected}, relations


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
    pointers = pointer_sites(semantic_coverage)
    assert {(row["source"], row["line"]) for row in pointers} == {
        ("caller", 5), ("address_target", 10), ("parenthesized_target", 15)}
    assert all(row["target"].endswith("fp") and row["signature"] == "void (*)()"
               for row in pointers), pointers
    assert_pointer_relations(semantic_coverage, pointers)


@then("uncertain function pointers have typed sites without guessed targets")
def uncertain_edges(semantic_coverage):
    project = semantic_coverage
    lines = [number for number, line in enumerate(
        project.source.read_text(encoding="utf-8").splitlines(), start=1)
        if "// unresolved" in line]
    callers = {"parameter", "reassigned", "escaped", "aliased", "captured"}
    pointers = pointer_sites(project)
    assert len(pointers) == len(callers), pointers
    assert {row["source"] for row in pointers} == callers, pointers
    assert {row["line"] for row in pointers} == set(lines), pointers
    assert all(row["signature"] == "void (*)()" and row["target_id"] is not None
               and row["target"].endswith("fp") for row in pointers), pointers
    assert not unresolved_sites(project), unresolved_sites(project)
    assert not diagnostics(project), project.last.stderr
    guessed = [row for row in call_sites(project)
               if row[0] in callers and row[1] in {"target", "other"}]
    assert not guessed, guessed
    assert_pointer_relations(project, pointers)
    if hasattr(project, "pre_migration_pointers"):
        assert pointers == project.pre_migration_pointers, pointers
        with sqlite3.connect(project.facts) as database:
            assert database.execute("PRAGMA user_version").fetchall() == [(14,)]


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


@then("pointer call shapes preserve their typed sites and declaration relations")
def pointer_shapes(semantic_coverage):
    project = semantic_coverage
    pointers = pointer_sites(project)
    by_source = {row["source"]: row for row in pointers}
    expected = {
        "parameter": "fp", "known_local": "fp", "reassigned": "fp",
        "field": "callback", "global": "global_callback", "array": None,
        "factory_expression": None, "conditional": None,
        "member_pointer": "method", "variadic": "fp", "nonthrowing": "fp",
        "function_reference": "fp"}
    assert len(pointers) == len(expected) and set(by_source) == set(expected), pointers
    source_lines = project.source.read_text(encoding="utf-8").splitlines()
    for name, target in expected.items():
        row = by_source[name]
        assert f"// pointer {name}" in source_lines[row["line"] - 1], row
        assert row["file_id"] > 0 and row["offset"] > 0 and row["col"] > 0, row
        assert row["expression"] and row["expression"] in source_lines[row["line"] - 1], row
        if target is None:
            assert row["target_id"] is None and row["target"] is None, row
        else:
            assert row["target_id"] and row["target"].endswith(target), row
            assert row["target_usr"] and not row["target_external"], row
    signatures = {name: row["signature"] for name, row in by_source.items()}
    ordinary = set(expected) - {"member_pointer", "variadic", "nonthrowing", "function_reference"}
    assert all(signatures[name] == "int (*)(double, const char *)"
               for name in ordinary), signatures
    assert signatures["member_pointer"] == "int (Receiver::*)(double) const", signatures
    assert signatures["variadic"] == "int (*)(const char *, ...)", signatures
    assert signatures["nonthrowing"] == "int (*)(double) noexcept", signatures
    assert signatures["function_reference"] == "int (&)(double, const char *)", signatures
    assert not unresolved_sites(project), unresolved_sites(project)
    assert_pointer_relations(project, pointers)
    direct = {(row[0], row[1]) for row in call_sites(project)}
    assert {("known_local", "local_target"), ("factory_expression", "factory"),
            ("known_external", "external_target")} <= direct, direct
    assert not any(source == "reassigned" and target in {"local_target", "alternative"}
                   for source, target in direct), direct


def semantic_entry(project, name):
    command = [str(project.tool), "analyse", "call-graph-entry", "--conf", str(project.conf),
               "--facts", str(project.facts), "--function", name, "--format", "json", "-v", "0"]
    project.run("entry", command=command)
    project.succeed()
    return json.loads(project.last.stdout)


@then("pointer call entries separate pointer evidence from external function targets")
def pointer_entry_queries(semantic_coverage):
    project = semantic_coverage
    pointers = {row["source"]: row for row in pointer_sites(project)}
    for name, site in pointers.items():
        entry = semantic_entry(project, name)
        assert entry["entry_available"] and entry["is_leaf"] is False, entry
        assert entry["coverage"]["pointer_calls"] == 1, entry
        assert entry["coverage"]["unresolved_targets"] == 0, entry
        assert len(entry["pointer_calls"]) == 1, entry
        pointer = entry["pointer_calls"][0]
        assert pointer["kind"] == "pointer-call", pointer
        assert pointer["signature"] == site["signature"], pointer
        assert pointer["expression"] == site["expression"], pointer
        assert pointer["site"]["line"] == site["line"], pointer
        if site["target_id"] is None:
            assert pointer["target"] is None and pointer["site"]["target_id"] is None, pointer
        else:
            assert int(pointer["target"]["symbol_id"]) == site["target_id"], pointer
            assert pointer["target"]["usr"] == site["target_usr"], pointer
        if name != "factory_expression":
            assert entry["external_targets"] == [], entry
    external = semantic_entry(project, "known_external")
    assert external["external_targets"] and external["pointer_calls"] == [], external
    assert external["coverage"]["pointer_calls"] == 0, external
    assert external["coverage"]["unresolved_targets"] == 0, external


def semantic_run(project, name):
    command = [str(project.tool), "analyse", "call-graph", "--conf", str(project.conf),
               "--facts", str(project.facts), "--function", name, "--recover-missing", "-v", "0"]
    project.run("call-graph", command=command)
    project.succeed()
    run_id, status = completion(project.last)
    assert status == "complete", project.last.stdout + project.last.stderr
    return run_id


def run_pointer_sites(project, run_id):
    with sqlite3.connect(project.facts) as database:
        database.row_factory = sqlite3.Row
        return [dict(row) for row in database.execute(
            "SELECT * FROM callgraph_run_pointer_call_site WHERE run_id=? "
            "ORDER BY source_id,file_id,offset", (run_id,))]


@then("persisted pointer call runs contain only reached caller evidence")
def pointer_run_queries(semantic_coverage):
    project = semantic_coverage
    pointers = {row["source"]: row for row in pointer_sites(project)}
    for root, reached in (("parameter", {"parameter"}), ("array", {"array"}),
                          ("reachable_chain", {"parameter", "known_local"})):
        run_id = semantic_run(project, root)
        snapshot = run_pointer_sites(project, run_id)
        assert len(snapshot) == len(reached), snapshot
        expected = {pointers[name]["source_id"]: pointers[name] for name in reached}
        assert {row["source_id"] for row in snapshot} == set(expected), snapshot
        for row in snapshot:
            site = expected[row["source_id"]]
            assert all(row[key] == site[key] for key in
                       ("target_id", "file_id", "offset", "line", "col", "signature", "expression")), row
            assert row["target_name"] == site["target"], row
            assert row["target_usr"] == site["target_usr"], row
        with sqlite3.connect(project.facts) as database:
            edges = database.execute(
                "SELECT source_id,destination_id,kind FROM callgraph_run_edge WHERE run_id=?",
                (run_id,)).fetchall()
            assert all(row[2] == 1 for row in edges), edges
            assert not database.execute(
                "SELECT 1 FROM callgraph_run_recovery WHERE run_id=? AND outcome IN ('attempted','failed')",
                (run_id,)).fetchall()
            assert not database.execute(
                "SELECT 1 FROM callgraph_run_frontier WHERE run_id=?", (run_id,)).fetchall()
        if root != "reachable_chain":
            assert edges == [], edges
        else:
            assert len(edges) == 3, edges


@when("semantic pointer facts are downgraded to version thirteen unresolved evidence")
def legacy_pointer_facts(semantic_coverage):
    project = semantic_coverage
    project.pre_migration_pointers = pointer_sites(project)
    with sqlite3.connect(project.facts) as database:
        database.execute(
            "INSERT INTO callgraph_unresolved_site(source_id,file_id,offset,line,col) "
            "SELECT source_id,file_id,offset,line,col FROM callgraph_pointer_call_site")
        database.execute("DELETE FROM relation_site WHERE kind=24")
        database.execute("DELETE FROM relation WHERE kind=24")
        database.execute("DROP TABLE callgraph_pointer_call_site")
        database.execute("DROP TABLE callgraph_run_pointer_call_site")
        database.execute("PRAGMA user_version=13")


@when("the semantic pointer call expressions are removed and re-extracted")
def remove_pointer_expressions(semantic_coverage):
    project = semantic_coverage
    project.historical_pointer_run = semantic_run(project, "parameter")
    project.historical_pointer_sites = run_pointer_sites(project, project.historical_pointer_run)
    assert len(project.historical_pointer_sites) == 1, project.historical_pointer_sites
    project.old_pointer_targets = {row["target_id"] for row in pointer_sites(project)
                                   if row["target_id"] is not None}
    source_lines = project.source.read_text(encoding="utf-8").splitlines()
    project.source.write_text("\n".join(
        "    return 0;" if "// pointer " in line else line for line in source_lines) + "\n",
        encoding="utf-8")
    project.commit_inputs()
    project.run("extract")
    project.succeed()


@then("obsolete pointer sites and relations are removed while value symbols survive")
def obsolete_pointer_facts(semantic_coverage):
    project = semantic_coverage
    assert not pointer_sites(project), pointer_sites(project)
    assert not unresolved_sites(project), unresolved_sites(project)
    with sqlite3.connect(project.facts) as database:
        assert not database.execute("SELECT 1 FROM relation WHERE kind=24").fetchall()
        assert not database.execute("SELECT 1 FROM relation_site WHERE kind=24").fetchall()
        symbols = {row[0] for row in database.execute("SELECT id FROM symbol")}
    assert project.old_pointer_targets <= symbols, project.old_pointer_targets - symbols
    assert run_pointer_sites(project, project.historical_pointer_run) == project.historical_pointer_sites
    assert run_pointer_sites(project, semantic_run(project, "parameter")) == []
    entry = semantic_entry(project, "parameter")
    assert entry["entry_available"] and entry["is_leaf"] is True, entry
    assert entry["pointer_calls"] == [] and entry["coverage"]["pointer_calls"] == 0, entry
