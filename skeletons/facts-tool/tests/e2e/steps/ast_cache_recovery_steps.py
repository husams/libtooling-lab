"""Call graph recovery scans consume persisted ASTs in separate CLI runs."""
import json

from pytest_bdd import given, then, when

from support.ast_cache_git import initialize_repository
from support.recovery import edge_names, graph, prepare, run, seed_match, success


@given("a recovery project has import-prepared ASTs and a first recovery scan")
def warm_recovery(context):
    prepare(context)
    initialize_repository(context.run_root_path / "recovery", context.recovery_env)
    cache = context.run_root_path / "recovery-asts"
    config = context.run_root_path / "recovery-cache.yaml"
    config.write_text(json.dumps({"ast_cache": True, "ast_cache_dir": str(cache)}),
                      encoding="utf-8")
    context.recovery_env["FACTS_TOOL_CONFIG"] = str(config)
    imported = run(context, "import", "-v", "1", "--conf", context.files_database_path,
                   "--facts", context.facts_database_path, "-p", context.run_root_path / "recovery")
    success(imported)
    assert imported.stderr.count("ast-cache: stored") == 3, imported.stderr
    artifacts = {path: (path.read_bytes(), path.stat().st_mtime_ns) for path in cache.rglob("*.ast")}
    assert len(artifacts) == 3, imported.stderr
    seed_match(context)
    first, first_run = graph(context, recover=True, verbosity=1)
    success(first)
    assert ("bridge", "leaf") in edge_names(first_run), first_run
    assert "ast-cache: hit" in first.stderr, first.stderr
    assert "ast-cache: miss" not in first.stderr, first.stderr
    assert "ast-cache: stored" not in first.stderr, first.stderr
    assert {path: (path.read_bytes(), path.stat().st_mtime_ns) for path in cache.rglob("*.ast")} == artifacts


@when("call graph recovery scans the cached project again")
def run_again(context):
    context.ast_cache_recovery_result, context.ast_cache_recovery_run = graph(
        context, recover=True, verbosity=1)


@then("call graph recovery reuses the persisted AST and preserves recovered edges")
def recovery_hit(context):
    result = context.ast_cache_recovery_result
    success(result)
    assert "ast-cache: hit" in result.stderr, result.stderr
    assert {("root", "bridge"), ("bridge", "leaf")} <= edge_names(context.ast_cache_recovery_run)
