"""Project access regressions for cold, repaired, and disabled AST consumers."""
from pytest_bdd import given, then, when

from support.ast_cache_assertions import require_miss, require_stored, require_symbol
from support.ast_cache_metadata import require_imported_artifact, require_normalized_inputs
from support.ast_cache_permissions import read_only_project, run_without_project_writes


@then("the AST consumer persists valid project cache metadata")
def persisted_metadata(ast_cache):
    require_miss(ast_cache)
    require_stored(ast_cache)
    require_normalized_inputs(ast_cache)
    require_imported_artifact(ast_cache)
    assert "readonly database" not in ast_cache.last.stderr.lower(), ast_cache.last.stderr


@given("the AST project database is read-only", target_fixture="ast_cache")
def disabled_read_only_project(context):
    with read_only_project(context, cache_enabled=False) as project:
        yield project


@when("an AST matcher with no results runs against the project")
def unmatched(ast_cache):
    command = ast_cache.command("match")
    command[command.index("--matcher") + 1] = (
        'functionDecl(hasName("cache_no_such_function")).bind("symbol")'
    )
    run_without_project_writes(ast_cache, "match", command=command)


@then("the matcher leaves the project database unchanged")
def unchanged_project(ast_cache):
    ast_cache.succeed()
    assert ast_cache.conf.read_bytes() == ast_cache.project_bytes_before
    assert "readonly database" not in ast_cache.last.stderr.lower(), ast_cache.last.stderr


@given("an enabled AST cache with a filesystem-read-only project database",
       target_fixture="ast_cache")
def unwritable_cache_project(context):
    with read_only_project(context, cache_enabled=True) as project:
        yield project


@when("extraction rebuilds the missing AST without write access to the project")
def rebuild_without_project_writes(ast_cache):
    for artifact in ast_cache.ast_files():
        artifact.unlink()
    run_without_project_writes(ast_cache, "extract")


@then("extraction succeeds while reporting the unavailable cache metadata")
def optional_cache_write(ast_cache):
    require_miss(ast_cache)
    require_symbol(ast_cache, "cache_root")
    diagnostics = ast_cache.last.stderr
    assert "ast-cache: unavailable" in diagnostics, diagnostics
    assert "readonly database" in diagnostics.lower(), diagnostics
    assert "ast-cache: stored" not in diagnostics, diagnostics
    assert ast_cache.conf.read_bytes() == ast_cache.project_bytes_before
