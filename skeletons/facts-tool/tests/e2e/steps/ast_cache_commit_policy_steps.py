"""Git HEAD is the explicit source refresh boundary for persisted ASTs."""
import shutil

from pytest_bdd import given, parsers, then, when

from support.ast_cache_assertions import fact_snapshot, require_hit, require_miss, require_stored
from support.ast_cache_git import commit_inputs, git, initialize_repository


@when(parsers.parse('the uncommitted "{input_kind}" becomes invalid and "{family}" runs'))
def dirty_input(ast_cache, input_kind, family):
    commit = git(ast_cache.root, ast_cache.environment, "rev-parse", "HEAD")
    path = ast_cache.source if input_kind == "source" else ast_cache.header
    with path.open("a", encoding="utf-8") as file:
        file.write("\n#error AST_CACHE_UNCOMMITTED_INPUT\n")
    assert git(ast_cache.root, ast_cache.environment, "rev-parse", "HEAD") == commit
    ast_cache.run(family)


@then("the same commit reuses the original AST and semantic facts without rewriting its artifact")
def unchanged_commit(ast_cache):
    require_hit(ast_cache)
    assert "dependency-cache: miss" not in ast_cache.last.stderr
    assert "AST_CACHE_UNCOMMITTED_INPUT" not in ast_cache.last.stderr
    assert fact_snapshot(ast_cache) == ast_cache.baseline
    assert ast_cache.snapshot_cache() == ast_cache.cache_before


@when("Git HEAD advances without changing the translation unit inputs")
def advance_head(ast_cache):
    before = git(ast_cache.root, ast_cache.environment, "rev-parse", "HEAD")
    assert ast_cache.commit_inputs(allow_empty=True) != before
    ast_cache.run("extract")


@then("the new commit rebuilds the AST with unchanged semantic facts")
def rebuild_unchanged_inputs(ast_cache):
    require_miss(ast_cache)
    require_stored(ast_cache)
    assert fact_snapshot(ast_cache) == ast_cache.baseline


@when("the persisted AST file disappears before extraction")
def missing_artifact(ast_cache):
    files = ast_cache.ast_files()
    assert files
    for path in files:
        path.unlink()
    ast_cache.run("extract")


@then("the missing AST is rebuilt while dependency metadata is reused")
def missing_rebuilt(ast_cache):
    require_miss(ast_cache)
    require_stored(ast_cache)
    assert "dependency-cache: hit" in ast_cache.last.stderr, ast_cache.last.stderr
    assert fact_snapshot(ast_cache) == ast_cache.baseline
    ast_cache.run("extract")
    require_hit(ast_cache)


@given("the source is outside any Git repository that tracks it")
def untracked_project(ast_cache):
    shutil.rmtree(ast_cache.root / ".git")


@then("extraction succeeds repeatedly without persisting an AST for the non-Git source")
def nongit_fallback(ast_cache):
    for _ in range(2):
        ast_cache.succeed()
        assert "ast-cache: hit" not in ast_cache.last.stderr
        assert "ast-cache: stored" not in ast_cache.last.stderr
        assert not ast_cache.ast_files()
        assert not list(ast_cache.cache.rglob("*.json"))
        ast_cache.run("extract")
    ast_cache.succeed()


@given("the committed translation unit uses compiler time macros")
def time_macros(ast_cache):
    with ast_cache.source.open("a", encoding="utf-8") as source:
        source.write('const char* cache_build_stamp = __DATE__ " " __TIME__ " " __TIMESTAMP__;\n')
    ast_cache.commit_inputs()


@given("a committed header is included from a separate Git repository")
def separate_header_repository(ast_cache):
    external = ast_cache.root.parent / "dependency-repository"
    external.mkdir()
    (external / "external.hpp").write_text("struct CacheExternalBefore {};\n", encoding="utf-8")
    initialize_repository(external, ast_cache.environment)
    ast_cache.source.write_text('#include <external.hpp>\n' + ast_cache.source.read_text(), encoding="utf-8")
    ast_cache.commit_inputs()
    ast_cache.configure(ast_cache=True, extra_args=["-I", str(external)])
    ast_cache.run("import")
    ast_cache.succeed()


@when("only the external header repository advances to a new commit")
def external_commit(ast_cache):
    source_head = git(ast_cache.root, ast_cache.environment, "rev-parse", "HEAD")
    external = ast_cache.root.parent / "dependency-repository"
    with (external / "external.hpp").open("a", encoding="utf-8") as header:
        header.write("struct CacheExternalCommitted {};\n")
    commit_inputs(external, ast_cache.environment)
    assert git(ast_cache.root, ast_cache.environment, "rev-parse", "HEAD") == source_head
    ast_cache.run("extract")


@given("an initially untracked header is included from another committed repository")
def untracked_external_header(ast_cache):
    external = ast_cache.root.parent / "dependency-repository"
    external.mkdir()
    (external / "anchor.hpp").write_text("struct CacheRepositoryAnchor {};\n", encoding="utf-8")
    initialize_repository(external, ast_cache.environment)
    (external / "external.hpp").write_text("struct CacheExternalUntracked {};\n", encoding="utf-8")
    assert not git(external, ast_cache.environment, "ls-files", "--", "external.hpp")
    ast_cache.source.write_text('#include <external.hpp>\n' + ast_cache.source.read_text(), encoding="utf-8")
    ast_cache.commit_inputs()
    ast_cache.configure(ast_cache=True, extra_args=["-I", str(external)])
    ast_cache.run("import")
    ast_cache.succeed()


@given("a committed dependency repository is searched for an absent optional header")
def optional_external_header(ast_cache):
    external = ast_cache.root.parent / "dependency-repository"
    includes = external / "include"
    includes.mkdir(parents=True)
    (includes / "anchor.hpp").write_text("struct CacheSearchRepositoryAnchor {};\n", encoding="utf-8")
    initialize_repository(external, ast_cache.environment)
    assert not (includes / "optional.hpp").exists()
    body = ('#if __has_include(<optional.hpp>)\n#include <optional.hpp>\n'
            '#else\nstruct CacheExternalOptionalAbsent {};\n#endif\n')
    ast_cache.source.write_text(body + ast_cache.source.read_text(), encoding="utf-8")
    ast_cache.commit_inputs()
    ast_cache.configure(ast_cache=True, extra_args=["-I", str(includes)])
    ast_cache.run("import")
    ast_cache.succeed()


@when("a dependency repository commit adds the optional header and the registry is refreshed")
def commit_optional_external_header(ast_cache):
    source_head = git(ast_cache.root, ast_cache.environment, "rev-parse", "HEAD")
    external = ast_cache.root.parent / "dependency-repository"
    (external / "include/optional.hpp").write_text("struct CacheExternalOptionalCommitted {};\n",
                                                 encoding="utf-8")
    commit_inputs(external, ast_cache.environment)
    assert git(ast_cache.root, ast_cache.environment, "rev-parse", "HEAD") == source_head
    ast_cache.run("import")
    ast_cache.succeed()
    assert "dependency-cache: miss" in ast_cache.last.stderr, ast_cache.last.stderr
    ast_cache.run("extract")


@given("the committed translation unit wraps compiler time macros in a macro")
def wrapped_time_macros(ast_cache):
    with ast_cache.source.open("a", encoding="utf-8") as source:
        source.write('#define ID(X) X\n'
                     'const char* cache_build_stamp = ID(__DATE__ " " __TIME__ " " __TIMESTAMP__);\n')
    ast_cache.commit_inputs()
