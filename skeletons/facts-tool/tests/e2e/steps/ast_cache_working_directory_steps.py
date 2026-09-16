"""Native cache reuse must preserve the compilation directory across invocations."""
from pytest_bdd import given, then, when

from support.ast_cache_assertions import require_symbol
from support.database import file_snapshot, query


@given("an explicitly configured AST cache project with relative nested includes")
def nested_include_project(ast_cache):
    includes = ast_cache.root / "relative-includes"
    (includes / "nested").mkdir(parents=True)
    (includes / "outer.hpp").write_text('#include "nested/detail.hpp"\n', encoding="utf-8")
    (includes / "nested/detail.hpp").write_text("struct CacheNestedCwd { int value; };\n",
                                              encoding="utf-8")
    ast_cache.source.write_text('#include <outer.hpp>\n' + ast_cache.source.read_text(),
                                encoding="utf-8")
    ast_cache.configure(tier="explicit", ast_cache=True,
                        ast_cache_dir=str(ast_cache.root / "configured-asts"),
                        extra_args=["-I", "relative-includes"])
    ast_cache.run("import")
    ast_cache.succeed()


@when("warm extraction runs from another process directory using absolute selectors")
def run_from_another_directory(ast_cache):
    invocation = ast_cache.root.parent / "other-invocation"
    decoy = invocation / "relative-includes"
    decoy.mkdir(parents=True)
    (decoy / "outer.hpp").write_text("#error AST_CACHE_USED_INVOCATION_CWD\n", encoding="utf-8")
    assert invocation != ast_cache.root
    assert all(path.is_absolute() for path in
               (ast_cache.source, ast_cache.conf, ast_cache.facts,
                ast_cache.selected_config, ast_cache.cache))
    ast_cache.run("extract", cwd=invocation)


@then("the original nested include symbols and registered header paths are retained")
def nested_include_evidence(ast_cache):
    ast_cache.succeed()
    require_symbol(ast_cache, "CacheNestedCwd")
    outer = query(ast_cache.conf, "SELECT id FROM file WHERE name='outer.hpp'")
    detail = query(ast_cache.conf, "SELECT id FROM file WHERE name='detail.hpp'")
    assert len(outer) == len(detail) == 1
    paths = dict(file_snapshot(ast_cache.conf))
    assert paths[outer[0][0]] == str(ast_cache.root / "relative-includes/outer.hpp")
    assert paths[detail[0][0]] == str(ast_cache.root / "relative-includes/nested/detail.hpp")
    assert "AST_CACHE_USED_INVOCATION_CWD" not in ast_cache.last.stderr
