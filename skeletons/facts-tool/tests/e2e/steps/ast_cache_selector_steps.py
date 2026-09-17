"""Source selectors share the cache identity prepared from stored commands."""
import json

from pytest_bdd import given, parsers, then, when

from support.ast_cache_assertions import require_symbol
from support.ast_cache_metadata import snapshot
from support.database import query


@given("import prepares two translation units built outside the invocation directory")
def imported_selector_project(ast_cache):
    build = ast_cache.root / "build"
    build.mkdir()
    nested = ast_cache.root / "nested"
    nested.mkdir()
    shared, leaf = (nested / name for name in ("shared.hpp", "leaf.hpp"))
    shared.write_text('#pragma once\n#include "leaf.hpp"\nstruct CacheShared {};\n', encoding="utf-8")
    leaf.write_text("#pragma once\nstruct CacheTransitive {};\n", encoding="utf-8")
    ast_cache.header.write_text('#include "nested/shared.hpp"\n' + ast_cache.header.read_text(),
                               encoding="utf-8")
    other = ast_cache.root / "cache_other.cpp"
    other.write_text('#include "cache.hpp"\nint cache_other() { return cache_adjust(2); }\n',
                     encoding="utf-8")
    ast_cache.commit_inputs()
    commands = [{"directory": str(build), "file": str(source),
                 "arguments": [str(ast_cache.compiler), "-std=c++23", "-c", str(source),
                               "-o", str(build / (source.stem + ".o"))]}
                for source in (ast_cache.source, other)]
    (ast_cache.root / "compile_commands.json").write_text(json.dumps(commands), encoding="utf-8")
    ast_cache.run("import", command=ast_cache.command("import")[:-1])
    ast_cache.succeed()
    assert ast_cache.last.stderr.count("ast-cache: stored") == 2, ast_cache.last.stderr
    ast_cache.selector_metadata = snapshot(ast_cache)
    ast_cache.selector_artifacts = ast_cache.snapshot_cache()
    assert len(ast_cache.ast_files()) == 2
    rows = ast_cache.selector_metadata
    assert len(rows["ast_cache_snapshot"]) == 2, rows
    for key, source, directory, _generation in rows["ast_cache_snapshot"]:
        assert source in (str(ast_cache.source), str(other))
        assert directory == str(build)
        assert {(key, str(ast_cache.header)), (key, str(shared)), (key, str(leaf))} <= set(
            rows["ast_cache_input"])
        assert {(key, source, str(ast_cache.header)),
                (key, str(ast_cache.header), str(shared)),
                (key, str(shared), str(leaf))} <= set(rows["ast_cache_include"])


@given("uncommitted compile errors guard the imported inputs against reparsing")
def guard_imported_inputs(ast_cache):
    for path in (*ast_cache.root.glob("*.cpp"), ast_cache.header,
                 *ast_cache.root.glob("nested/*.hpp")):
        with path.open("a", encoding="utf-8") as source:
            source.write("\n#error AST_CACHE_SELECTOR_UNEXPECTED_REPARSE\n")


def selector_arguments(project, selector):
    if selector == "all sources":
        return [], project.root
    if selector == "multiple absolute":
        return [str(project.source), str(project.root / "cache_other.cpp")], project.root
    if selector == "duplicate absolute":
        return [str(project.source), str(project.source)], project.root
    if selector == "overlapping files":
        return [str(project.source), project.source.name], project.root
    if selector == "dot relative file":
        return ["./" + project.source.name], project.root
    if selector == "nested invocation":
        invocation = project.root / "invocation" / "nested"
        invocation.mkdir(parents=True)
        return ["../../cache.cpp"], invocation
    assert selector in ("relative file", "absolute file"), selector
    return [project.source.name if selector == "relative file" else str(project.source)], project.root


@when(parsers.parse('the "{family}" consumer runs twice with "{selector}" source selection'))
def repeated_selector_consumer(ast_cache, family, selector):
    arguments, invocation = selector_arguments(ast_cache, selector)
    command = ast_cache.command(family)[:-1] + arguments
    ast_cache.selector_count = 2 if selector in ("all sources", "multiple absolute") else 1
    ast_cache.selector_observations = []
    for _ in range(2):
        result = ast_cache.run(family, command=command, cwd=invocation)
        ast_cache.selector_observations.append((result, snapshot(ast_cache),
                                                ast_cache.snapshot_cache()))


@then("both selector invocations reuse the imported cache without rebuilding")
def selector_cache_hits(ast_cache):
    for result, metadata, artifacts in ast_cache.selector_observations:
        assert result.returncode == 0, result.stdout + result.stderr
        diagnostics = result.stderr
        if ast_cache.last_family in ("extract", "dependency"):
            assert diagnostics.count("dependency-cache: hit") == ast_cache.selector_count, diagnostics
        assert "AST_CACHE_SELECTOR_UNEXPECTED_REPARSE" not in diagnostics, diagnostics
        for event in ("dependency-cache: miss", "dependency-cache: unavailable",
                      "dependency-cache: stored", "ast-cache: miss", "ast-cache: unavailable",
                      "ast-cache: stored", "frontend: ast-parse", "frontend: dependency-scan",
                      "frontend: include-reconstruction", "frontend: driver-probe"):
            assert event not in diagnostics, diagnostics
        if ast_cache.last_family != "dependency":
            assert diagnostics.count("ast-cache: hit") == ast_cache.selector_count, diagnostics
        else:
            assert "ast-cache:" not in diagnostics, diagnostics
        assert metadata == ast_cache.selector_metadata
        assert artifacts == ast_cache.selector_artifacts


@then("the imported shared and transitive header metadata remains unchanged")
def unchanged_selector_includes(ast_cache):
    assert snapshot(ast_cache) == ast_cache.selector_metadata
    assert ast_cache.snapshot_cache() == ast_cache.selector_artifacts
    assert not query(ast_cache.conf, "PRAGMA foreign_key_check")
    if ast_cache.last_family == "extract":
        require_symbol(ast_cache, "CacheShared")
        require_symbol(ast_cache, "CacheTransitive")
    if ast_cache.last_family == "dependency":
        headers = dict(query(ast_cache.conf, "SELECT name,id FROM file "
                             "WHERE name IN ('cache.hpp','shared.hpp','leaf.hpp')"))
        for source, target in (("cache.hpp", "shared.hpp"), ("shared.hpp", "leaf.hpp")):
            assert query(ast_cache.facts, "SELECT 1 FROM include_dependency "
                         "WHERE src_file_id=? AND dst_file_id=?",
                         (headers[source], headers[target]))


@when(parsers.parse('the guarded "{family}" consumer runs with caching disabled'))
def disabled_selector_consumer(ast_cache, family):
    ast_cache.configure(ast_cache=False)
    ast_cache.run(family)


@then("the uncached consumer reports the input reparse guard")
def reparse_guard_detected(ast_cache):
    assert "AST_CACHE_SELECTOR_UNEXPECTED_REPARSE" in ast_cache.last.stderr, ast_cache.last.stderr
    assert "ast-cache: hit" not in ast_cache.last.stderr, ast_cache.last.stderr
    assert "dependency-cache: hit" not in ast_cache.last.stderr, ast_cache.last.stderr
