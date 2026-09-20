"""Compiler diagnostics stay in typed job errors and never leak raw log lines."""
import json

from pytest_bdd import given, parsers, then, when

from domain_http import completed, eventually, index_ready, submit


@given("a repository-aware daemon with a configured JSON log")
def daemon(domain_catalog, rest_server):
    domain_catalog.log = rest_server.root / "domain.jsonl"
    rest_server.start([*domain_catalog.options(), "--daemon", "--log-file",
                       domain_catalog.log, "--log-level", "trace"])
    index_ready(rest_server.api)


@when(parsers.parse('I submit invalid C++ through the typed "{operation}" API'))
def invalid_cpp(domain_catalog, rest_server, operation):
    source = domain_catalog.sources["alpha"]
    source.write_text("int broken( { return 1; }\n")
    options = {"query": 'functionDecl().bind("selected")'} if operation == "matches" else {}
    job = submit(rest_server.api, f"/v1/{operation}", {"path": str(source)}, **options)
    domain_catalog.diagnostic_job = rest_server.api.wait(job["id"])


@then("the failed domain job contains structured compiler diagnostics")
def diagnostics(domain_catalog, rest_server):
    job = domain_catalog.diagnostic_job
    assert job["state"] == "failed" and job["result"] is None, job
    assert "stdout" not in job and "stderr" not in job
    diagnostics = job["error"]["details"]["diagnostics"]
    assert any(item["severity"] in {"error", "fatal"} and item["message"]
               for item in diagnostics), diagnostics
    assert any(item["file"] == str(domain_catalog.sources["alpha"]) and item["line"] == 1
               for item in diagnostics), diagnostics
    assert rest_server.api.request("GET", "/health")[0] == 200


@then("every daemon log line remains a valid structured JSON event")
def json_lines(domain_catalog):
    def records():
        rows = [json.loads(line) for line in domain_catalog.log.read_text().splitlines()]
        return rows if any(row["event"] == "job.completed" and row["fields"].get("job_id") == (
            domain_catalog.diagnostic_job["id"]) for row in rows) else None
    assert all({"timestamp", "level", "event", "fields"}.issubset(row)
               for row in eventually(records))


@when(parsers.parse('I complete valid C++ analysis through the typed "{operation}" API'))
def successful(domain_catalog, rest_server, operation):
    source = domain_catalog.sources["alpha"]
    domain_catalog.write_source(source, "alpha", "daemon")
    options = {"query": 'functionDecl(hasName("alpha::daemon")).bind("symbol")'} if (
        operation == "matches") else {}
    job = submit(rest_server.api, f"/v1/{operation}", {"path": str(source)}, **options)
    domain_catalog.diagnostic_job = completed(rest_server.api, job)
