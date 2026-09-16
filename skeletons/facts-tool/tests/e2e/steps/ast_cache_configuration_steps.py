"""Configuration precedence is verified by the cache location actually used."""
from pytest_bdd import given, parsers, then

from support.ast_cache_assertions import require_stored


@given(parsers.parse('AST cache configuration is selected from "{tier}"'))
def tier_settings(ast_cache, tier):
    ast_cache.configure(tier="user", ast_cache=True,
                        ast_cache_dir=str(ast_cache.root / "user-asts"))
    if tier in ("project", "explicit"):
        ast_cache.configure(ast_cache=True, ast_cache_dir=str(ast_cache.root / "project-asts"))
    if tier == "explicit":
        ast_cache.configure(tier="explicit", ast_cache=True,
                            ast_cache_dir=str(ast_cache.root / "explicit-asts"))


@then(parsers.parse('only the "{tier}" configuration cache is populated'))
def chosen_tier(ast_cache, tier):
    require_stored(ast_cache)
    for candidate in ("user", "project", "explicit"):
        path = ast_cache.root / f"{candidate}-asts"
        assert path.is_dir() == (candidate == tier), str(path)


@given("explicit configuration disables a project enabled cache")
def explicit_disable(ast_cache):
    ast_cache.configure(ast_cache=True)
    ast_cache.configure(tier="explicit", ast_cache=False)


@given("the configured AST cache directory is relative")
def relative_path(ast_cache):
    ast_cache.configure(ast_cache=True, ast_cache_dir="relative-asts")


@then("the relative AST cache directory is populated under the project")
def relative_used(ast_cache):
    require_stored(ast_cache)
    assert ast_cache.cache == ast_cache.root / "relative-asts"


@then("configuration inspection reports the AST settings without creating the cache")
def inspection(ast_cache):
    ast_cache.succeed()
    assert "ast_cache: true" in ast_cache.last.stdout, ast_cache.last.stdout
    assert "ast_cache_dir:" in ast_cache.last.stdout, ast_cache.last.stdout
    assert str(ast_cache.cache) in ast_cache.last.stdout, ast_cache.last.stdout
    assert not ast_cache.cache.exists()
