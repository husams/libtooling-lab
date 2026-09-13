from __future__ import annotations

import json
import subprocess
from pathlib import Path

from facts_tool import open_codebase
from pytest_bdd import given, then, when
from support.scenario import FactsToolContext

BUILTIN_RECORD_USR = "c:@S@__va_list_tag"


def _prepare(context: FactsToolContext, fixture: str, stem: str, extra: list[str]) -> Path:
    context.prepare()
    source = (context.fixture_root / fixture).resolve(strict=True)
    context.facts_database = context.run_root_path / f"{stem}.sqlite"
    context.files_database = context.run_root_path / f"{stem}-project.sqlite"
    command = [str(context.compiler), *extra, "-c", str(source)]
    (context.run_root_path / "compile_commands.json").write_text(
        json.dumps([{"directory": str(context.fixture_root), "file": str(source),
                     "arguments": command}], indent=2) + "\n",
        encoding="utf-8",
    )
    return source


@given("a B-022 stream compile database", target_fixture="b022_source")
def given_stream_database(context: FactsToolContext) -> Path:
    return _prepare(context, "b022_call_site_invalid_usr.cpp", "b022-stream", ["-std=c++23"])


@given("a B-022 builtin compile database", target_fixture="b022_source")
def given_builtin_database(context: FactsToolContext) -> Path:
    return _prepare(
        context,
        "b022_call_site_invalid_usr_builtin.cpp",
        "b022-builtin",
        ["--target=x86_64-unknown-linux-gnu", "-nostdinc", "-nostdinc++", "-std=c++23"],
    )


def _extract(context: FactsToolContext, source: Path, force: bool) -> None:
    imported = subprocess.run(
        [str(context.facts_tool), "import", "--conf", str(context.files_database_path),
         "--facts", str(context.facts_database_path), "--compilation-database",
         str(context.run_root_path), str(source)], capture_output=True, text=True, check=False,
    )
    assert imported.returncode == 0, imported.stdout + imported.stderr
    extract = [str(context.facts_tool), "extract", "--output",
               str(context.facts_database_path), "--conf", str(context.files_database_path),
               "--verbose", "3", str(source)]
    if force:
        extract.insert(2, "--force")
    completed = subprocess.run(extract, capture_output=True, text=True, check=False)
    context.last_returncode = completed.returncode
    context.last_output = completed.stdout + completed.stderr
    context.b022_runs.append((completed.returncode, context.last_output))


def _snapshot(context: FactsToolContext) -> dict[str, tuple[int, str]]:
    with open_codebase(facts_db=context.facts_database_path,
                       project_db=context.files_database_path) as codebase:
        return {entity.usr: (entity.id, entity.qualified_name)
                for entity in codebase.query().nodes().all() if entity.usr}


@when("B-022 extraction indexes the fixture")
def when_extracts_once(context: FactsToolContext, b022_source: Path) -> None:
    context.b022_runs = []
    _extract(context, b022_source, force=False)


@when("B-022 extraction indexes the fixture twice")
def when_extracts_twice(context: FactsToolContext, b022_source: Path) -> None:
    context.b022_runs = []
    _extract(context, b022_source, force=False)
    context.b022_before = _snapshot(context)
    _extract(context, b022_source, force=True)
    context.b022_after = _snapshot(context)


@then("B-022 extraction commits without incomplete diagnostics")
def then_commits(context: FactsToolContext) -> None:
    for returncode, output in context.b022_runs:
        assert returncode == 0, output
        assert "indexing incomplete" not in output, output
        assert "cannot extract callable invocation" not in output, output
        assert "rollback output transaction" not in output, output


def _open_symbols(context: FactsToolContext):
    return open_codebase(facts_db=context.facts_database_path,
                         project_db=context.files_database_path)


@then("the B-022 canary and sibling Calls are persisted")
def then_stream_calls(context: FactsToolContext, b022_source: Path) -> None:
    with _open_symbols(context) as codebase:
        canary = codebase.get("b022::canary")
        sibling = codebase.get("b022::sibling")
        assert any(callee.usr == sibling.usr for callee in canary.callees())
        sites = codebase.graph.references(sibling.usr)
        assert any(site["source_id"] == canary.id and site["kind"] == "calls"
                   and site["file"] == str(b022_source) and site["line"] > 0
                   for site in sites)


@then("the B-022 builtin record and constructor call sites are canonical")
def then_builtin_calls(context: FactsToolContext, b022_source: Path) -> None:
    assert context.b022_before == context.b022_after
    with _open_symbols(context) as codebase:
        canary = codebase.get("b022::canary")
        sibling = codebase.get("b022::sibling")
        builtin = codebase.get(BUILTIN_RECORD_USR)
        assert builtin.usr == BUILTIN_RECORD_USR
        assert builtin.qualified_name == "__va_list_tag"
        assert builtin.kind == "struct"
        assert builtin.identity["file_id"] == 0 and builtin.identity["index"] > 0
        same_name = [entity for entity in codebase.query().nodes().all()
                     if entity.qualified_name == "__va_list_tag"]
        assert len(same_name) == 1 and same_name[0].usr == BUILTIN_RECORD_USR
        assert any(callee.usr == sibling.usr for callee in canary.callees())
        constructors = [callee for callee in canary.callees()
                        if callee.usr.startswith(BUILTIN_RECORD_USR + "@F@")
                        and callee.kind == "constructor"]
        assert constructors
        for constructor in constructors:
            sites = [site for site in codebase.graph.references(constructor.usr)
                     if site["source_id"] == canary.id and site["kind"] == "calls"]
            assert sites
            assert all(site["destination_id"] == constructor.id
                       and site["receiver_type_id"] == builtin.id
                       and site["file"] == str(b022_source) and site["line"] > 0
                       for site in sites)
        returned = codebase.get("b022::returns_builtin")
        return_targets = returned.outgoing("return_type")
        assert len(return_targets) == 1 and return_targets[0].usr == builtin.usr
