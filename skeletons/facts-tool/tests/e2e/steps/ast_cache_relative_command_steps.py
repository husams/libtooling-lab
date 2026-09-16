"""Imported and stored command spellings share one dependency-cache identity."""
import json

from pytest_bdd import given, then

from support.ast_cache_assertions import require_symbol
from support.ast_cache_metadata import require_dependency_stored, snapshot
from support.database import file_snapshot


@given("dependency metadata is imported from a relative source and joined relative include option")
def relative_compile_command(ast_cache):
    headers = ast_cache.root / "headers"
    headers.mkdir()
    (headers / "relative.hpp").write_text("struct CacheRelativeCommand {};\n", encoding="utf-8")
    ast_cache.source.write_text('#include <relative.hpp>\n' + ast_cache.source.read_text(), encoding="utf-8")
    ast_cache.commit_inputs()
    database = ast_cache.root / "compile_commands.json"
    commands = json.loads(database.read_text())
    command = commands[0]
    substitutions = {str(ast_cache.source): ast_cache.source.name,
                     str(ast_cache.root / "cache.o"): "cache.o"}
    command["file"] = ast_cache.source.name
    command["output"] = "cache.o"
    command["arguments"] = [substitutions.get(argument, argument)
                            for argument in command["arguments"]]
    command["arguments"].insert(2, "-Iheaders")
    database.write_text(json.dumps(commands), encoding="utf-8")
    ast_cache.run("import")
    require_dependency_stored(ast_cache)
    rows = snapshot(ast_cache)["ast_cache_snapshot"]
    assert len(rows) == 1, rows
    ast_cache.relative_snapshot_key = rows[0][0]
    assert rows[0][1] == str(ast_cache.source)


@then("relative and stored command spellings retain the single imported dependency snapshot")
def single_command_identity(ast_cache):
    ast_cache.succeed()
    assert "dependency-cache: hit" in ast_cache.last.stderr, ast_cache.last.stderr
    assert "dependency-cache: miss" not in ast_cache.last.stderr, ast_cache.last.stderr
    rows = snapshot(ast_cache)["ast_cache_snapshot"]
    assert len(rows) == 1 and rows[0][0] == ast_cache.relative_snapshot_key, rows


@then("the relative include header is represented by the extracted facts")
def relative_header_fact(ast_cache):
    require_symbol(ast_cache, "CacheRelativeCommand")
    assert str(ast_cache.root / "headers/relative.hpp") in dict(file_snapshot(ast_cache.conf)).values()
