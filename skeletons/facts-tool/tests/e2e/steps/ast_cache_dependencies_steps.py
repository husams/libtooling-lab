"""Compiler-provided includes and dependency lookup changes invalidate caches."""
import os
import sqlite3

from pytest_bdd import given, parsers, then, when

from support.ast_cache_assertions import require_hit, require_miss, require_stored, require_symbol, require_dependency_hit
from support.database import query


def reimport(project, *arguments):
    project.write_commands(*arguments)
    project.commit_inputs()
    project.run("import")
    project.succeed()


@given("the cached translation unit uses a compiler forced include")
def forced_include(ast_cache):
    forced = ast_cache.root / "forced.hpp"
    (ast_cache.root / "forced_nested.hpp").write_text(
        "struct CacheForcedNested { int nested; };\n", encoding="utf-8")
    forced.write_text('#include "forced_nested.hpp"\n'
                      "struct CacheForcedInclude { int value; };\n", encoding="utf-8")
    reimport(ast_cache, "-include", str(forced))


@when(parsers.parse('the forced include consumer "{family}" runs with fresh output'))
def fresh_forced_consumer(ast_cache, family):
    ast_cache.forced_family = family
    ast_cache.forced_conf = ast_cache.conf
    if family == "import":
        with sqlite3.connect(ast_cache.conf) as database:
            database.execute("DELETE FROM file WHERE name IN ('forced.hpp', 'forced_nested.hpp')")
    ast_cache.forced_facts = ast_cache.root / "forced-facts.sqlite"
    substitutions = {str(ast_cache.conf): str(ast_cache.forced_conf),
                     str(ast_cache.facts): str(ast_cache.forced_facts)}
    command = [substitutions.get(arg, arg) for arg in ast_cache.command(family)]
    ast_cache.run(family, command=command)


@then("the cached include graph retains the compiler forced header")
def forced_graph(ast_cache):
    require_dependency_hit(ast_cache)
    rows = query(ast_cache.forced_conf, "SELECT id FROM file WHERE name='forced.hpp'")
    assert len(rows) == 1, rows
    nested = query(ast_cache.forced_conf, "SELECT id FROM file WHERE name='forced_nested.hpp'")
    assert len(nested) == 1, nested
    if ast_cache.forced_family == "dependency":
        assert query(ast_cache.forced_facts,
                     "SELECT src_file_id FROM include_dependency WHERE src_file_id=? AND dst_file_id=?",
                     (rows[0][0], nested[0][0]))


@given("an include resolves through a lower priority search directory")
def search_directories(ast_cache):
    first, second = (ast_cache.root / name for name in ("first", "second"))
    first.mkdir()
    second.mkdir()
    (second / "search.hpp").write_text("struct CacheSearchBefore {};\n", encoding="utf-8")
    ast_cache.source.write_text('#include <search.hpp>\n' + ast_cache.source.read_text(),
                                encoding="utf-8")
    reimport(ast_cache, "-I", str(first), "-I", str(second))


@when("a header appears in the higher priority search directory and the registry is refreshed")
def shadow_header(ast_cache):
    (ast_cache.root / "first/search.hpp").write_text("struct CacheSearchShadow {};\n",
                                                    encoding="utf-8")
    ast_cache.commit_inputs()
    ast_cache.run("import")
    require_miss(ast_cache)
    require_stored(ast_cache)
    ast_cache.run("extract")


@given("the source conditionally includes a header that does not exist")
def optional_header(ast_cache):
    body = ('#if __has_include("optional.hpp")\n#include "optional.hpp"\n'
            '#else\nstruct CacheOptionalAbsent {};\n#endif\n')
    ast_cache.source.write_text(body + ast_cache.source.read_text(), encoding="utf-8")
    reimport(ast_cache)


@when("the optional header becomes available and the registry is refreshed")
def optional_available(ast_cache):
    (ast_cache.root / "optional.hpp").write_text("struct CacheOptionalAvailable {};\n",
                                                encoding="utf-8")
    ast_cache.commit_inputs()
    ast_cache.run("import")
    require_miss(ast_cache)
    require_stored(ast_cache)
    ast_cache.run("extract")


@given("the compiler arguments come from a response file")
def response_arguments(ast_cache):
    response = ast_cache.root / "compile.rsp"
    response.write_text("-DCACHE_VALUE=7\n", encoding="utf-8")
    reimport(ast_cache, "@" + str(response))


@when("the compiler response file enables a different source declaration and is reimported")
def response_changed(ast_cache):
    (ast_cache.root / "compile.rsp").write_text("-DCACHE_VALUE=7 -DCACHE_MODE=1\n", encoding="utf-8")
    ast_cache.commit_inputs()
    ast_cache.run("import")
    require_miss(ast_cache)
    require_stored(ast_cache)
    ast_cache.refreshed_by_import = True
    ast_cache.run("extract")


@then(parsers.parse('the changed include lookup exposes "{symbol}"'))
def lookup_changed(ast_cache, symbol):
    require_hit(ast_cache)
    require_symbol(ast_cache, symbol)
    ast_cache.run("extract")
    require_hit(ast_cache)


@when(parsers.parse('the cached "{input_kind}" acquires a committed compile error and "{family}" runs'))
def compile_error(ast_cache, input_kind, family):
    path = ast_cache.source if input_kind == "source" else ast_cache.header
    with path.open("a", encoding="utf-8") as file:
        file.write("\n#error AST_CACHE_FRESH_INPUT_FAILURE\n")
    ast_cache.commit_inputs()
    ast_cache.run(family)


@then("the command reports the fresh compile error instead of using the old AST")
def fresh_error(ast_cache):
    assert ast_cache.last.returncode != 0, ast_cache.last.stdout + ast_cache.last.stderr
    assert "AST_CACHE_FRESH_INPUT_FAILURE" in ast_cache.last.stderr, ast_cache.last.stderr
    assert "ast-cache: hit" not in ast_cache.last.stderr


@given("an include search path traverses a symlink followed by its parent")
def physical_include_path(ast_cache):
    physical = ast_cache.root / "physical"
    (physical / "child").mkdir(parents=True)
    headers = physical / "headers"
    headers.mkdir()
    (headers / "cache_physical.hpp").write_text("struct CachePhysicalBefore {};\n", encoding="utf-8")
    decoy = ast_cache.root / "headers"
    decoy.mkdir()
    (decoy / "cache_physical.hpp").write_text("struct CacheLexicalDecoy {};\n", encoding="utf-8")
    alias = ast_cache.root / "alias"
    alias.symlink_to(physical / "child", target_is_directory=True)
    include = alias / ".." / "headers"
    assert include.resolve() == headers.resolve()
    assert os.path.normpath(include) == str(decoy)
    ast_cache.source.write_text('#include <cache_physical.hpp>\n' + ast_cache.source.read_text(),
                                encoding="utf-8")
    # Runtime defaults preserve the include spelling independently of imported
    # compile-option path normalization, which is outside the cache contract.
    ast_cache.configure(ast_cache=True, extra_args=["-I", str(include)])
    reimport(ast_cache)


@then("the physically resolved header symbol is present")
def physical_symbol(ast_cache):
    require_symbol(ast_cache, "CachePhysicalBefore")


@when("the physical header behind the include path is committed without a timestamp change")
def physical_header_changed(ast_cache):
    header = ast_cache.root / "physical/headers/cache_physical.hpp"
    before = header.stat()
    header.write_text(header.read_text().replace("CachePhysicalBefore", "CachePhysicalAfter_"),
                      encoding="utf-8")
    assert header.stat().st_size == before.st_size
    os.utime(header, ns=(before.st_atime_ns, before.st_mtime_ns))
    ast_cache.commit_inputs()
    ast_cache.run("extract")
