"""Compiler failures retain the selected clone, command, and source diagnostic."""
from contract import response_matches
from domain_http import index_ready
from logging_support import records
from support import eventually


def test_preparation_failure_reports_compiler_context_and_logs(
        domain_project, server_factory, tmp_path):
    project = domain_project
    source = project.sources["alpha"]
    source.write_text('#include "missing-build-header.hpp"\nint broken();\n')
    log = tmp_path / "errors.jsonl"
    server = server_factory(*project.options(), "--log-file", log)
    index_ready(server.api)
    status, job = server.api.request("POST", "/api/v2/extract/job", {
        "selection": {"type": "files", "files": [
            {"repository": "alpha", "path": "src/main.cpp"}]}})
    assert status == 202, job
    location = f"/api/v2/extract/job/{job['id']}"

    def failed():
        status, current = server.api.request("GET", location)
        assert status == 200, current
        return current if current["state"] == "failed" else None

    result = eventually(failed)
    error = result["error"]
    assert "missing-build-header.hpp" in error["message"], error
    details = error["details"]
    assert details["path"] == str(source)
    assert details["repository"] == "alpha"
    assert details["clone_path"] == str(project.roots["alpha"])
    assert details["project_root"] == str(project.roots["alpha"])
    command, = details["compilation_commands"]
    assert command["source_file"] == str(source)
    assert command["working_directory"] == str(project.roots["alpha"])
    assert command["driver"] == project.compiler
    assert command["arguments"][0] == project.compiler
    assert str(source) in command["arguments"]
    diagnostic = next(d for d in details["diagnostics"]
                      if "missing-build-header.hpp" in d["message"])
    assert diagnostic["file"] == str(source)
    assert diagnostic["line"] == 1 and diagnostic["column"] > 0
    _, document = server.api.request("GET", "/openapi.json")
    response_matches(document, "GET", "/api/v2/extract/job/{id}", 200, result)

    def logged():
        return next((r for r in records(log) if r["event"] == "job.failed"
                     and r["fields"]["job_id"] == job["id"]), None)

    failure = eventually(logged)["fields"]
    assert failure["path"] == str(source)
    assert failure["clone_path"] == str(project.roots["alpha"])
    assert failure["working_directories"] == [str(project.roots["alpha"])]
    assert "missing-build-header.hpp" in failure["message"]
    assert any(r["event"] == "job.diagnostic" and
               r["fields"].get("file") == str(source) for r in records(log))

    assert details["stage"]
    assert details["expected"] and "retry" in details["action"]
    assert details["server_working_directory"] == str(server.root)
    assert details["project_database"] == str(project.database)
    assert isinstance(details["environment"], dict)
    effective = details["effective_compilation_commands"]
    assert effective and any("resource-dir" in arg for c in effective for arg in c["arguments"])
    _, listing = server.api.request("GET", "/api/v2/extract/job")
    listed = next(item for item in listing["items"] if item["id"] == job["id"])
    assert listed["error"] == error
    context_records = [r["fields"] for r in records(log)
                       if r["event"] == "job.context" and r["fields"]["job_id"] == job["id"]]
    parts = sorted((r for r in context_records if r["key"] == "effective_compilation_commands"),
                   key=lambda r: r["part"])
    import json
    assert json.loads("".join(r["value"] for r in parts)) == effective
