"""BDD steps for cache lifecycle and shared AST consumers."""
import pytest
from pytest_bdd import given, parsers, then, when

from support.ast_cache import AstCacheProject
from support.ast_cache_assertions import (fact_snapshot, require_consumer_result,
                                          require_hit, require_miss, require_stored,
                                          require_consumer_cache_hit)


@pytest.fixture
def ast_cache(context, tmp_path):
    return AstCacheProject.create(context, tmp_path / "ast-cache-project")


@given("an isolated AST cache project")
def isolated(ast_cache):
    pass


@given("AST caching is enabled")
def enabled(ast_cache):
    ast_cache.configure(ast_cache=True)


@given("AST caching is explicitly disabled")
def disabled(ast_cache):
    ast_cache.configure(ast_cache=False)


@given("AST caching uses a custom directory")
def custom(ast_cache):
    ast_cache.configure(ast_cache=True, ast_cache_dir=str(ast_cache.root / "custom-asts"))


@given("a persisted AST from extraction")
def persisted(ast_cache):
    ast_cache.run("extract")
    require_stored(ast_cache)
    ast_cache.baseline = fact_snapshot(ast_cache)
    ast_cache.cache_before = ast_cache.snapshot_cache()


@when(parsers.parse('the AST cache project runs "{family}"'))
def run(ast_cache, family):
    ast_cache.run(family)


@when("AST caching is disabled after the cache was populated")
def disable_populated(ast_cache):
    ast_cache.configure(ast_cache=False)
    ast_cache.run("extract")


@then("the cold AST is persisted")
def cold(ast_cache):
    require_miss(ast_cache)
    require_stored(ast_cache)
    ast_cache.baseline = fact_snapshot(ast_cache)


@then("the persisted AST is reused")
def reused(ast_cache):
    require_hit(ast_cache)


@then("cold and warm extraction facts are identical")
def identical(ast_cache):
    assert fact_snapshot(ast_cache) == ast_cache.baseline


@then(parsers.parse('the cached "{family}" result is complete'))
def complete(ast_cache, family):
    require_consumer_result(ast_cache, family)


@then("the cache is untouched and emits no cache events")
def untouched(ast_cache):
    ast_cache.succeed()
    assert "ast-cache:" not in ast_cache.last.stderr, ast_cache.last.stderr
    assert "dependency-cache:" not in ast_cache.last.stderr, ast_cache.last.stderr
    assert ast_cache.snapshot_cache() == ast_cache.cache_before


@then("no AST cache directory is created")
def no_directory(ast_cache):
    ast_cache.succeed()
    assert not ast_cache.cache.exists()
    assert "ast-cache:" not in ast_cache.last.stderr, ast_cache.last.stderr
    assert "dependency-cache:" not in ast_cache.last.stderr, ast_cache.last.stderr


@then("only the configured AST cache directory is populated")
def only_custom(ast_cache):
    require_stored(ast_cache)
    assert not (ast_cache.root / ".facts-tool/ast-cache").exists()


@then("the default project AST cache directory is populated")
def default_directory(ast_cache):
    require_stored(ast_cache)
    assert ast_cache.cache == ast_cache.root / ".facts-tool/ast-cache"


@when(parsers.parse('AST caching is disabled and "{family}" runs against the populated cache'))
def disabled_consumer(ast_cache, family):
    ast_cache.configure(ast_cache=False)
    ast_cache.run(family)


@then("the configured cache is reused by the command")
def consumer_cache_reused(ast_cache):
    require_consumer_cache_hit(ast_cache)
