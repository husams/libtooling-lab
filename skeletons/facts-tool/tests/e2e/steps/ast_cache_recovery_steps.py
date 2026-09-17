"""Call graph recovery scans consume persisted ASTs in separate CLI runs."""
import json
import sqlite3

from pytest_bdd import given, then, when

from support.ast_cache_git import initialize_repository
from support.recovery import edge_names, graph, prepare, run, seed_match, success


def assert_reused_without_frontend_work(result):
    success(result)
    assert "ast-cache: hit" in result.stderr, result.stderr
    assert "ast-cache: miss" not in result.stderr, result.stderr
    assert "ast-cache: stored" not in result.stderr, result.stderr
    for activity in ("ast-parse", "dependency-scan", "include-reconstruction"):
        assert f"frontend: {activity} " not in result.stderr, result.stderr


@given("a recovery project has import-prepared ASTs and a first recovery scan")
def warm_recovery(context):
    prepare(context)
    prepare_cached_recovery(context)


@given("cached recovery uses a symlink-parent header and an unrelated source is missing")
def warm_symlink_recovery(context):
    prepare(context)
    root = context.run_root_path / "recovery"
    physical = root / "physical"
    (physical / "child").mkdir(parents=True)
    headers = physical / "headers"
    headers.mkdir()
    header = headers / "input.hpp"
    header.write_text("#define RECOVERY_VALUE 19\n", encoding="utf-8")
    alias = root / "alias"
    alias.symlink_to(physical / "child", target_is_directory=True)
    raw_header = alias / ".." / "headers" / "input.hpp"
    assert raw_header.resolve() == header.resolve()
    decoy = root / "headers"
    decoy.mkdir()
    (decoy / "input.hpp").write_text("#error LEXICAL_HEADER_MUST_NOT_BE_USED\n",
                                     encoding="utf-8")
    source = context.recovery_sources[1]
    source.write_text(source.read_text().replace('"input.hpp"', f'"{raw_header}"'),
                      encoding="utf-8")
    prepare_cached_recovery(context, missing_alternative=True,
                            expected_inputs={str(raw_header), str(header.resolve())})


def prepare_cached_recovery(context, *, missing_alternative=False, expected_inputs=()):
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
    if expected_inputs:
        with sqlite3.connect(context.files_database_path) as database:
            inputs = {row[0] for row in database.execute(
                "SELECT path FROM ast_cache_input WHERE snapshot_key IN "
                "(SELECT key FROM ast_cache_snapshot WHERE source=?)",
                (str(context.recovery_sources[1]),))}
        assert expected_inputs <= inputs, inputs
    seed_match(context)
    if missing_alternative:
        # Falling back to every registered input would include this missing
        # unrelated TU and prevent recovery's content digest from succeeding.
        context.recovery_alternative.unlink()
    first, first_run = graph(context, recover=True, verbosity=1)
    assert_reused_without_frontend_work(first)
    assert ("bridge", "leaf") in edge_names(first_run), first_run
    assert {path: (path.read_bytes(), path.stat().st_mtime_ns) for path in cache.rglob("*.ast")} == artifacts


@when("call graph recovery scans the cached project again")
def run_again(context):
    context.ast_cache_recovery_result, context.ast_cache_recovery_run = graph(
        context, recover=True, verbosity=1)


@then("call graph recovery reuses the persisted AST and preserves recovered edges")
def recovery_hit(context):
    result = context.ast_cache_recovery_result
    assert_reused_without_frontend_work(result)
    assert {("root", "bridge"), ("bridge", "leaf")} <= edge_names(context.ast_cache_recovery_run)
