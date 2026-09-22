"""Acceptance tests against the instance started by scripts/start-server.sh.

Run with .server-runtime/venv/bin/python -m pytest tests/server -v.
Temporary registrations are removed; evidence stays under .server-runtime/evidence.
"""

import asyncio
from dataclasses import asdict
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import uuid
from urllib.parse import quote

import httpx
import pytest
import yaml
from facts_tool.rest import (
    ApiError, AsyncClient, Client, CompilationCommand, DeclarationLocation,
    FileIdentity, FileReference, FileSelection, JobFailed, NewClone, RepositorySelection,
    SymbolReference, VariableReference,
)

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / ".server-runtime"
EVIDENCE = RUNTIME / "evidence" / ("acceptance-" + time.strftime("%Y%m%dT%H%M%S"))
EVIDENCE.mkdir(parents=True, exist_ok=True)


def save(name, value):
    (EVIDENCE / f"{name}.json").write_text(json.dumps(value, indent=2, default=str) + "\n")


def wait(job, timeout=180):
    try:
        return job.wait(timeout=timeout)
    finally:
        save(job.id, {"metadata": asdict(job.metadata),
                      "result": asdict(job.result) if job.result else None})


def import_source(api, sample, source):
    database = sample["directory"] / "compile_commands.json"
    commands = json.loads(database.read_text())
    commands.append({"directory": str(sample["directory"]), "file": str(source),
                     "arguments": [shutil.which("clang++"), "-std=c++17", "-c", str(source)]})
    database.write_text(json.dumps(commands))
    wait(api.imports.create(repository=sample["name"], compilation_database=str(database)))
    return next(f for f in api.files.list(repository=sample["name"]) if f.path == str(source))


@pytest.fixture(scope="module")
def api():
    config = yaml.safe_load((RUNTIME / "server.yaml").read_text())
    with Client(f"http://{config['host']}:{config['port']}",
                token=os.environ.get("FACTS_TOOL_API_TOKEN"), timeout=60) as client:
        yield client


@pytest.fixture(scope="module")
def sample(api):
    previous = api.watcher.settings()
    api.watcher.update_settings(enabled=False)
    name = "acceptance-" + uuid.uuid4().hex[:8]
    directory = Path(tempfile.mkdtemp(prefix=name + "-"))
    save("fixture-location", {"repository": name, "directory": str(directory)})
    subprocess.run(["git", "init", "-q", str(directory)], check=True)
    header = directory / "common.hpp"
    header.write_text("#pragma once\nnamespace probe { int leaf(int value); }\n")
    source = directory / f"main-{name}.cpp"
    source.write_text('#include "common.hpp"\nnamespace probe {\nint run() {\n'
                      '  int tracked = 3;\n  return leaf(tracked);\n}\n}\n')
    leaf = directory / f"leaf-{name}.cpp"
    leaf.write_text('#include "common.hpp"\nnamespace probe { int leaf(int value) { return value + 1; } }\n')
    compiler = shutil.which("clang++")
    commands = [{"directory": str(directory), "file": str(path),
                 "arguments": [compiler, "-std=c++17", "-c", str(path)]}
                for path in (source, leaf)]
    (directory / "compile_commands.json").write_text(json.dumps(commands))
    repo = api.repositories.create(name=name, clones=[NewClone(str(directory), "test")])
    try:
        api.components.create(name=name, path=str(directory), repository=name)
        wait(api.imports.create(repository=name, compilation_database=str(directory / "compile_commands.json")))
        wait(api.extractions.create(selection=RepositorySelection(name), force=True))
        yield {"name": name, "repo": repo, "directory": directory, "source": source, "leaf": leaf,
               "selection": FileSelection([FileReference(source.name, repository=name)])}
    finally:
        try:
            api.repositories.delete(repo.id, cascade=True)
            wait(api.index.create())
        finally:
            api.watcher.replace_settings(previous)


def test_real_repository_ready(api):
    assert api.server.health().status == "ok"
    repo = next(r for r in api.repositories.list() if r.name == "facts-tool")
    save("repository", asdict(repo))
    assert repo.source_count >= 129
    assert repo.indexed_source_count == repo.source_count
    assert api.index.status().state == "ready"
    assert next(iter(api.symbols.find("facts::", repository="facts-tool", limit=1)), None)


def test_real_repository_analysis(api):
    selection = FileSelection([FileReference("src/config/ConfigurationMerge.cpp", repository="facts-tool")])
    extracted = wait(api.extractions.create(selection=selection, force=True))
    assert extracted.files_processed == 1 and extracted.symbols_written > 0
    match = api.matches.create(selection=selection,
        expression='functionDecl(hasName("mergeTiers")).bind("chosen")', capture_source=True)
    assert wait(match).match_count >= 1
    assert api.matches.results(match.id).collect()[0].bindings["chosen"].node_kind
    dependency = api.dependencies.create(selection=selection)
    assert wait(dependency).edge_count > 0
    graph = api.callgraphs.create(root=SymbolReference(qualified_name="facts::config::detail::mergeTiers"), max_depth=2)
    assert wait(graph).node_count >= 2
    save("real-callgraph-nodes", [asdict(n) for n in api.callgraphs.nodes(graph.id)])
    flow = api.variable_flow.create(function=SymbolReference(qualified_name="facts::config::detail::mergeTiers"),
                                    variable=VariableReference("base"), selection=selection,
                                    interprocedural=False)
    assert wait(flow).node_count > 0


def test_catalog_and_symbol_queries(api, sample):
    repo = api.repositories.get(sample["repo"].id)
    assert repo.source_count == 2 and repo.indexed_source_count == 2
    files = api.files.list(repository=sample["name"], limit=1).collect()
    assert {sample["source"].name, sample["leaf"].name, "common.hpp"} <= {f.name for f in files}
    assert api.directories.list(repository=sample["name"], limit=1).collect()
    prefixed = api.symbols.find("probe::", repository=sample["name"], limit=1).collect()
    assert {"probe::run", "probe::leaf"} <= {s.qualified_name for s in prefixed}
    symbol, = api.symbols.find("probe::run", repository=sample["name"], match="exact").collect()
    assert api.symbols.get(symbol.symbol_id) == symbol
    assert api.symbols.find(usr=symbol.usr, repository=sample["name"]).collect() == [symbol]
    assert api.symbols.occurrences(symbol.symbol_id).collect()
    assert not api.symbols.find("probe::does_not_exist").collect()
    assert not api.symbols.find("PROBE::run").collect()
    assert not api.symbols.find("probe::%").collect()


def test_typed_symbol_relations(api, sample):
    symbol, = api.symbols.find("probe::run", repository=sample["name"], match="exact").collect()
    config = yaml.safe_load((RUNTIME / "server.yaml").read_text())
    headers = {"Authorization": "Bearer " + os.environ["FACTS_TOOL_API_TOKEN"]} if os.environ.get("FACTS_TOOL_API_TOKEN") else {}
    response = httpx.get(f"http://{config['host']}:{config['port']}/api/v2/symbols/"
                        + quote(symbol.symbol_id, safe="") + "/relations?direction=both", headers=headers)
    save("relations-wire", {"status": response.status_code, "body": response.json()})
    response.raise_for_status()
    save("relations", [asdict(r) for r in api.symbols.relations(symbol.symbol_id, direction="both")])


def test_file_creation_in_imported_component_root(api, sample):
    source = sample["directory"] / "api-added.cpp"
    source.write_text("int added_through_file_api() { return 42; }\n")
    save("file-create-input", {"path": str(source),
        "directories": [asdict(d) for d in api.directories.list(repository=sample["name"])],
        "components": [asdict(c) for c in api.components.list(repository=sample["name"])]})
    try:
        file = api.files.create(path=str(source), compilation_command=CompilationCommand(
            shutil.which("clang++"), str(sample["directory"]), ["-std=c++17"]))
    except ApiError as error:
        save("file-create-error", {"status": error.status_code, "code": error.code,
                                   "message": str(error), "details": error.details})
        raise
    assert file.path == str(source)
    api.files.delete(file.id, cascade=True)


def test_incremental_extraction_and_matching(api, sample):
    first = api.extractions.create(selection=sample["selection"], force=True)
    assert wait(first).files_processed == 1
    assert len(api.extractions.results(first.id, limit=1).collect()) == 1
    incremental = wait(api.extractions.create(selection=sample["selection"]))
    assert incremental.files_processed == 0 and incremental.files_skipped == 1
    matched = api.matches.create(selection=sample["selection"],
        expression='functionDecl(hasName("probe::run")).bind("arbitrary")',
        traversal="IgnoreUnlessSpelledInSource", capture_source=True)
    assert wait(matched).match_count == 1
    assert api.matches.results(matched.id).collect()[0].bindings["arbitrary"].node_kind
    empty = api.matches.create(selection=sample["selection"],
                              expression='functionDecl(hasName("absent")).bind("x")')
    assert wait(empty).match_count == 0
    assert matched.cancel().state == "succeeded"


def test_dependencies(api, sample):
    job = api.dependencies.create(selection=sample["selection"])
    assert wait(job).edge_count >= 1
    edges = api.dependencies.results(job.id, limit=1).collect()
    assert any(e.source_path == str(sample["source"]) and e.destination_path.endswith("common.hpp") for e in edges)
    save("dependencies", [asdict(e) for e in edges])


def test_invalid_matcher_and_backward_flow(api, sample):
    invalid = api.matches.create(selection=sample["selection"], expression="notARealMatcher()")
    with pytest.raises(JobFailed):
        wait(invalid)
    assert invalid.error.code and invalid.error.message
    with pytest.raises(ApiError) as backward:
        api.variable_flow.create(
            function=SymbolReference(qualified_name="probe::run", repository=sample["name"]),
            variable=VariableReference("tracked"), direction="backward")
    assert backward.value.status_code == 422


def test_shadowed_local_declaration(api, sample):
    source = sample["directory"] / f"shadow-{sample['name']}.cpp"
    source.write_text('namespace probe {\nint shadow(bool choose) {\n'
                      '  int tracked = 1;\n  if (choose) {\n'
                      '    int tracked = 2;\n    return tracked;\n  }\n'
                      '  return tracked;\n}\n}\n')
    file = import_source(api, sample, source)
    try:
        wait(api.extractions.create(selection=FileSelection([FileIdentity(file.id)]), force=True))
        function = SymbolReference(qualified_name="probe::shadow", repository=sample["name"])
        ambiguous = api.variable_flow.create(function=function, variable=VariableReference("tracked"))
        with pytest.raises(JobFailed):
            wait(ambiguous)
        for line, column in ((3, 7), (5, 9)):
            job = api.variable_flow.create(function=function,
                variable=VariableReference("tracked", DeclarationLocation(source.name, line, column)))
            assert wait(job).node_count > 0
            save(f"shadow-line-{line}", [asdict(n) for n in api.variable_flow.nodes(job.id)])
    finally:
        api.files.delete(file.id, cascade=True)
        database = sample["directory"] / "compile_commands.json"
        database.write_text(json.dumps([c for c in json.loads(database.read_text()) if c["file"] != str(source)]))
        wait(api.index.create())


@pytest.mark.parametrize("direction,root,expected", [
    ("callees", "probe::run", "probe::leaf"),
    ("callers", "probe::leaf", "probe::run"),
])
def test_callgraphs(api, sample, direction, root, expected):
    job = api.callgraphs.create(root=SymbolReference(qualified_name=root, repository=sample["name"]),
                               direction=direction, max_depth=5)
    assert wait(job).node_count >= 2
    assert expected in {n.qualified_name for n in api.callgraphs.nodes(job.id, limit=1)}
    assert api.callgraphs.edges(job.id, limit=1).collect()
    save("graph-" + direction, [asdict(n) for n in api.callgraphs.nodes(job.id)])


def test_graph_paths_and_limits(api, sample):
    root = SymbolReference(qualified_name="probe::run", repository=sample["name"])
    target = SymbolReference(qualified_name="probe::leaf", repository=sample["name"])
    job = api.callgraphs.create(root=root, target=target, path_mode="shortest")
    wait(job)
    assert api.callgraphs.paths(job.id).collect()
    limited = api.callgraphs.create(root=root, max_nodes=1)
    summary = wait(limited)
    assert summary.truncated or summary.coverage == "partial"


@pytest.mark.parametrize("interprocedural", [False, True])
def test_variable_tracking(api, sample, interprocedural):
    job = api.variable_flow.create(
        function=SymbolReference(qualified_name="probe::run", repository=sample["name"]),
        variable=VariableReference("tracked", DeclarationLocation(sample["source"].name, 4, 7)),
        interprocedural=interprocedural, max_call_depth=5)
    result = wait(job)
    assert result.node_count > 0
    nodes = api.variable_flow.nodes(job.id, limit=1).collect()
    edges = api.variable_flow.edges(job.id, limit=1).collect()
    if interprocedural:
        assert {"argument-copy", "return"} <= {e.kind for e in edges}
        assert any(n.name == "value" and n.depth == 1 for n in nodes)
    else:
        assert result.coverage == "partial"
        assert any(b.reason == "depth-limit" for b in api.variable_flow.boundaries(job.id))
    save("flow-" + str(interprocedural), {"nodes": [asdict(n) for n in nodes], "edges": [asdict(e) for e in edges]})


def test_cli_added_facts_database_and_rebuild(api, sample):
    source = sample["directory"] / f"manual-{sample['name']}.cpp"
    source.write_text("namespace probe { int manually_added() { return 91; } }\n")
    import_source(api, sample, source)
    facts = sample["directory"] / "manually-added-facts.db"
    command = [str(RUNTIME / "build/facts-tool"), "extract", "--force", "--conf",
               str(RUNTIME / "project.db"), "--config", str(RUNTIME / "defaults.yaml"),
               "-o", str(facts), str(source)]
    completed = subprocess.run(command, capture_output=True, text=True, timeout=90, cwd=sample["directory"])
    save("manual-cli", {"command": command, "exit_code": completed.returncode,
                        "stdout": completed.stdout, "stderr": completed.stderr})
    assert completed.returncode == 0 and facts.exists()
    save("before-manual-reindex", [asdict(s) for s in api.symbols.find("probe::manually_added")])
    indexed = wait(api.index.create())
    assert indexed.sources_processed >= 1
    assert len(api.symbols.find("probe::manually_added", match="exact").collect()) == 1
    unchanged = wait(api.index.create())
    assert unchanged.sources_processed == 0 and unchanged.index_revision == indexed.index_revision
    source.write_text("namespace probe { int manually_replaced() { return 92; } }\n")
    completed = subprocess.run(command, capture_output=True, text=True, timeout=90, cwd=sample["directory"])
    assert completed.returncode == 0
    wait(api.index.create())
    assert not api.symbols.find("probe::manually_added", match="exact").collect()
    assert api.symbols.find("probe::manually_replaced", match="exact").collect()


def test_failure_evidence_and_retry(api, sample):
    source = sample["source"]
    original = source.read_text()
    source.write_text('#include "intentional-missing-header.hpp"\n' + original)
    failed = api.extractions.create(selection=sample["selection"], force=True)
    try:
        with pytest.raises(JobFailed):
            wait(failed)
        assert failed.state == "failed"
        details = failed.error.details
        assert "intentional-missing-header.hpp" in json.dumps(details)
        assert details["diagnostics"] and details["compilation_commands"]
        assert details["clone_path"] == str(sample["directory"])
        deadline = time.monotonic() + 10
        records = []
        while time.monotonic() < deadline:
            records = [json.loads(line) for line in (RUNTIME / "logs/server.jsonl").read_text().splitlines()
                       if line.startswith("{")]
            if any(r["event"] == "job.failed" and r["fields"].get("job_id") == failed.id for r in records):
                break
            time.sleep(0.1)
        relevant = [r for r in records if r["fields"].get("job_id") == failed.id]
        save("failed-job-log-records", relevant)
        assert {"job.failed", "job.diagnostic"} <= {r["event"] for r in relevant}
    finally:
        source.write_text(original)
    retried = failed.retry()
    assert wait(retried).files_processed == 1
    assert retried.metadata.retry_of == failed.id
    assert api.extractions.get(failed.id).state == "failed"
    with pytest.raises(ApiError) as conflict:
        retried.retry()
    assert conflict.value.status_code == 409
    with pytest.raises(ApiError) as wrong_family:
        api.matches.get(failed.id)
    assert wrong_family.value.status_code == 404


def test_scan_and_job_history(api, sample):
    job = api.scans.create(selection=RepositorySelection(sample["name"]))
    assert wait(job).warning_count == 0
    assert api.scans.databases(job.id).collect()
    assert api.extractions.list(limit=1).collect()
    assert api.scans.get(job.id).state == "succeeded"


def test_async_client(api, sample):
    config = yaml.safe_load((RUNTIME / "server.yaml").read_text())
    async def scenario():
        async with AsyncClient(f"http://{config['host']}:{config['port']}",
                               token=os.environ.get("FACTS_TOOL_API_TOKEN")) as async_api:
            assert (await async_api.server.health()).status == "ok"
            symbols = await async_api.symbols.find("probe::", limit=1).collect()
            assert any(s.qualified_name == "probe::run" for s in symbols)
            job = await async_api.matches.create(selection=sample["selection"],
                expression='functionDecl(hasName("probe::run")).bind("async")')
            assert (await job.wait(timeout=60)).match_count >= 1
            assert await async_api.matches.results(job.id, limit=1).collect()
    asyncio.run(scenario())
