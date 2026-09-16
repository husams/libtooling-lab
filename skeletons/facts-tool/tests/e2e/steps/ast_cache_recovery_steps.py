"""Call graph recovery scans consume persisted ASTs in separate CLI runs."""
import json

from pytest_bdd import given, then, when

from support.recovery import edge_names, graph, prepare, seed_match, success


@given("a recovery project has AST caching enabled and a warmed recovery scan")
def warm_recovery(context):
    prepare(context)
    cache = context.run_root_path / "recovery-asts"
    config = context.run_root_path / "recovery-cache.yaml"
    config.write_text(json.dumps({"ast_cache": True, "ast_cache_dir": str(cache)}),
                      encoding="utf-8")
    context.recovery_env["FACTS_TOOL_CONFIG"] = str(config)
    seed_match(context)
    first, run = graph(context, recover=True, verbosity=1)
    success(first)
    assert ("bridge", "leaf") in edge_names(run), run
    assert tuple(cache.rglob("*.ast")), first.stdout + first.stderr


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
