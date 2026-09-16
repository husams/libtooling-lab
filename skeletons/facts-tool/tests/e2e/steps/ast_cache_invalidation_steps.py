"""Input freshness and unusable cache recovery through real CLI processes."""
import os

from pytest_bdd import given, parsers, then, when

from support.ast_cache_assertions import (require_hit, require_miss, require_stored,
                                          require_symbol)


@when(parsers.parse('the AST cache input "{input_kind}" changes'))
def change_input(ast_cache, input_kind):
    if input_kind == "source commit":
        with ast_cache.source.open("a", encoding="utf-8") as source:
            source.write("\nint cache_source_changed() { return 2; }\n")
    elif input_kind == "header commit":
        with ast_cache.header.open("a", encoding="utf-8") as header:
            header.write("\nstruct CacheHeaderChanged { int other; };\n")
    elif input_kind == "compiler arguments":
        ast_cache.write_commands("-DCACHE_MODE=1")
        ast_cache.run("import")
        ast_cache.succeed()
    elif input_kind == "header commit with preserved size and timestamp":
        before = ast_cache.header.stat()
        ast_cache.header.write_text(
            ast_cache.header.read_text(encoding="utf-8").replace("HeaderBefore", "HeaderAfter_"),
            encoding="utf-8")
        assert ast_cache.header.stat().st_size == before.st_size
        os.utime(ast_cache.header, ns=(before.st_atime_ns, before.st_mtime_ns))
    else:
        raise AssertionError(input_kind)
    if input_kind != "compiler arguments":
        ast_cache.commit_inputs()
    ast_cache.run("extract")


@then(parsers.parse('AST regeneration exposes the fresh symbol "{symbol}"'))
def fresh_symbol(ast_cache, symbol):
    require_miss(ast_cache)
    require_stored(ast_cache)
    require_symbol(ast_cache, symbol)
    ast_cache.run("extract")
    require_hit(ast_cache)
    require_symbol(ast_cache, symbol)


@when("the persisted AST bytes are corrupted and extraction runs")
def corrupt(ast_cache):
    files = ast_cache.ast_files()
    assert files
    for path in files:
        path.write_bytes(b"not a serialized clang AST\n")
    ast_cache.run("extract")


@then("the corrupt AST is rebuilt and reusable")
def rebuilt(ast_cache):
    require_stored(ast_cache)
    require_symbol(ast_cache, "cache_root")
    ast_cache.run("extract")
    require_hit(ast_cache)


@given("the configured cache directory is blocked by a regular file")
def blocked(ast_cache):
    blocked_path = ast_cache.root / "blocked-cache"
    blocked_path.write_text("existing user file", encoding="utf-8")
    ast_cache.configure(ast_cache=True, ast_cache_dir=str(blocked_path))


@then("extraction succeeds without damaging the blocking file")
def fallback(ast_cache):
    ast_cache.succeed()
    require_symbol(ast_cache, "cache_root")
    assert ast_cache.cache.read_text(encoding="utf-8") == "existing user file"
    assert "ast-cache: hit" not in ast_cache.last.stderr
    assert "ast-cache: stored" not in ast_cache.last.stderr
    ast_cache.run("extract")
    ast_cache.succeed()
    require_symbol(ast_cache, "cache_root")
