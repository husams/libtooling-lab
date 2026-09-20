"""Native v2 analysis resources: real interprocedural evidence, no CLI envelopes."""
import json
from urllib.parse import quote

import pytest

from domain_http import index_ready
from contract import response_matches
from support import eventually


def run(api, operation, body):
    status, headers, content = api.exchange("POST", f"/api/v2/{operation}/job", body)
    job = json.loads(content)
    assert status == 202, job
    location = headers.get("Location") or headers.get("location")
    assert location == f"/api/v2/{operation}/job/{job['id']}"
    _, document = api.request("GET", "/openapi.json")
    response_matches(document, "POST", f"/api/v2/{operation}/job", 202, job)

    def completed():
        code, current = api.request("GET", location)
        assert code == 200, current
        return current if current["state"] in {"succeeded", "failed", "cancelled"} else None

    completed_job = eventually(completed, timeout=60)
    assert completed_job["state"] == "succeeded", completed_job
    response_matches(document, "GET", f"/api/v2/{operation}/job/{{id}}", 200, completed_job)
    return completed_job, location


def rows(api, location, collection):
    result, cursor = [], None
    _, document = api.request("GET", "/openapi.json")
    schema_path = location.rsplit("/", 1)[0] + "/{id}/results"
    while True:
        suffix = f"/results?collection={collection}&limit=500"
        if cursor:
            suffix += "&cursor=" + quote(cursor, safe="")
        status, page = api.request("GET", location + suffix)
        assert status == 200, page
        response_matches(document, "GET", schema_path, status, page)
        result.extend(page["items"])
        cursor = page["next_cursor"]
        if cursor is None:
            return result


@pytest.fixture
def analysis_server(domain_project, server_factory):
    project = domain_project
    source = project.sources["alpha"]
    root = project.roots["alpha"]
    (source.parent / "common.hpp").write_text(
        "#pragma once\nnamespace alpha { int leaf(int value); }\n")
    source.write_text(
        '#include "common.hpp"\n'
        "namespace alpha {\n"
        "int run() {\n"
        "  int tracked = 3;\n"
        "  return leaf(tracked);\n"
        "}\n}\n")
    leaf = source.parent / "leaf.cpp"
    leaf.write_text('#include "common.hpp"\nnamespace alpha { int leaf(int value) { return value + 1; } }\n')
    (root / "compile_commands.json").write_text(json.dumps([
        {"directory": str(root), "file": str(path),
         "arguments": [project.compiler, "-std=c++17", "-c", str(path)]}
        for path in (source, leaf)
    ]))
    server = server_factory(*project.options())
    index_ready(server.api)
    imported, _ = run(server.api, "import", {"repository": "alpha"})
    assert imported["result"]["compilation_databases"] == 1
    extracted, _ = run(server.api, "extract", {
        "selection": {"type": "repository", "repository": "alpha"}})
    assert extracted["result"]["files_processed"] == 2
    assert extracted["result"]["index_revision"]
    return server


def test_callgraph_reaches_definition_in_another_translation_unit(analysis_server):
    api = analysis_server.api
    job, location = run(api, "callgraphs", {
        "root": {"qualified_name": "alpha::run"}, "direction": "callees"})
    result = job["result"]
    nodes = {node["symbol_id"]: node for node in rows(api, location, "nodes")}
    edge = next(edge for edge in rows(api, location, "edges")
                if nodes[edge["target"]]["qualified_name"] == "alpha::leaf")
    assert nodes[edge["source"]]["qualified_name"] == "alpha::run"
    assert nodes[edge["target"]]["definition"]
    assert isinstance(edge["file_id"], str)
    assert not result["truncated"]
    status, page = api.request("GET", location + "/results?collection=nodes&limit=1")
    assert status == 200 and len(page["items"]) == 1, page
    assert page["next_cursor"]
    # A repeated DELETE is harmless and never rewrites a completed outcome.
    for _ in range(2):
        code, retained = api.request("DELETE", location)
        assert code == 200 and retained["state"] == "succeeded", retained


def test_variable_flow_reports_argument_and_return_transfers(analysis_server):
    job, location = run(analysis_server.api, "variable-flow", {
        "function": {"qualified_name": "alpha::run"},
        "variable": {"name": "tracked", "declaration": {
            "path": "src/main.cpp", "line": 4, "column": 7}},
        "interprocedural": True, "max_call_depth": 10})
    result = job["result"]
    kinds = {edge["kind"] for edge in rows(analysis_server.api, location, "edges")}
    assert "argument-copy" in kinds and "return" in kinds, result
    nodes = rows(analysis_server.api, location, "nodes")
    assert any(node["name"] == "value" for node in nodes), result
    assert any(node["depth"] == 1 for node in nodes), result


def test_variable_flow_depth_boundary_is_explicit(analysis_server):
    job, location = run(analysis_server.api, "variable-flow", {
        "function": {"qualified_name": "alpha::run"},
        "variable": {"name": "tracked"}, "interprocedural": False})
    assert job["result"]["coverage"] == "partial"
    assert any(item["reason"] == "depth-limit" for item in rows(analysis_server.api, location, "boundaries"))


def test_unchanged_extraction_counts_skipped_files(analysis_server):
    selection = {"type": "files", "files": [{"path": "src/main.cpp", "repository": "alpha"}]}
    run(analysis_server.api, "extract", {"selection": selection})
    job, _ = run(analysis_server.api, "extract", {"selection": selection})
    assert job["result"]["files_processed"] == 0
    assert job["result"]["files_skipped"] == 1
    assert job["result"]["symbols_written"] == 0


def test_patched_compiler_command_registers_new_includes_without_reimport(domain_server, domain_project):
    api = domain_server.api
    header = domain_project.sources["alpha"].parent / "added.hpp"
    header.write_text("namespace alpha { struct AddedByPatch {}; }\n")
    status, listing = api.request("GET", "/api/v2/files?repository=alpha")
    assert status == 200, listing
    source = next(file for file in listing["items"] if file["name"] == "main.cpp")
    command = source["compilation_command"]
    command["arguments"] += ["-include", str(header)]
    status, patched = api.request("PATCH", f"/api/v2/files/{source['id']}", {
        "compilation_command": command})
    assert status == 200, patched
    run(api, "extract", {"selection": {"type": "files", "files": [{"file_id": source["id"]}]}})
    status, symbols = api.request("GET", "/api/v2/symbols?qualified_name=alpha%3A%3AAddedByPatch&match=exact")
    assert status == 200 and symbols["items"], symbols
    status, current = api.request("GET", f"/api/v2/files/{source['id']}")
    assert status == 200 and "-include" in current["compilation_command"]["arguments"]


@pytest.mark.parametrize("operation,body", [
    ("extract", {"selection": {"type": "all"}, "facts": "/tmp/client.db"}),
    ("extract", {"selection": {"type": "files", "files": [{"file_id": 12}]}}),
    ("callgraphs", {"root": {"qualified_name": "f"}, "max_nodes": -1}),
    ("callgraphs", {"root": {"qualified_name": "f", "usr": "u"}}),
    ("variable-flow", {"function": {"qualified_name": "f"},
                       "variable": {"name": "x"}, "direction": "backward"}),
    ("scan", {"selection": {"type": "files", "files": [{"file_id": "1"}]}}),
])
def test_analysis_validation_rejects_untyped_options_before_job_admission(server, operation, body):
    status, error = server.api.request("POST", f"/api/v2/{operation}/job", body)
    assert status == 422, error
    assert error["error"]["code"] == "invalid_request", error
