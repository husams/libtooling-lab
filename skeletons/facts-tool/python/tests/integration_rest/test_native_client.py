import json
import sys
from pathlib import Path

import pytest
from support.native_cli_helpers import compiler

from facts_tool import open_codebase
from facts_tool.rest import ApiError, Client, JobFailedError


def test_import_extract_match_then_query(rest_url: str, tmp_path: Path):
    source = tmp_path / "sample.cpp"
    source.write_text("int answer() { return 42; }\nint main() { return answer(); }\n")
    project, facts = tmp_path / "project.db", tmp_path / "facts.db"
    commands = [
        {
            "directory": str(tmp_path),
            "file": str(source),
            "arguments": [str(compiler()), "-std=c++17", "-c", str(source)],
        }
    ]
    (tmp_path / "compile_commands.json").write_text(json.dumps(commands))
    with Client(rest_url, token="integration-token") as client:
        imported = client.run("import", "-p", str(tmp_path), "-c", str(project))
        assert imported.succeeded and imported.done
        extracted = client.run("extract", "-c", str(project), "-o", str(facts))
        assert extracted.exit_code == 0 and not extracted.truncated
        matched = client.run(
            "match",
            "-c",
            str(project),
            "-f",
            str(facts),
            "--matcher",
            'functionDecl(hasName("answer")).bind("symbol")',
            str(source),
        )
        assert "answer" in (matched.stdout or "") + (matched.stderr or "")
        graph = client.command(
            "analyse/call-graph",
            "-c",
            str(project),
            "-f",
            str(facts),
            "--function",
            "main",
        )
        assert client.wait(graph.id, timeout=10).succeeded
    with open_codebase(facts_db=facts, project_db=project) as codebase:
        assert "answer" in codebase.query().nodes().names()


def test_native_metadata_and_failures(rest_url: str):
    with Client(rest_url, token="integration-token") as client:
        assert client.health()["status"] == "ok"
        assert any(item["path"] == "extract" for item in client.commands())
        assert "/v1/jobs" in client.openapi()["paths"]
        assert client.watch_status()["enabled"] is (sys.platform == "linux")
        job = client.submit("config", "show")
        assert client.wait(job.id, timeout=5).succeeded
        assert client.get_job(job.id).stdout
        listed = next(item for item in client.list_jobs() if item.id == job.id)
        assert listed.stdout is None
        assert client.cancel_job(job.id).succeeded
        with pytest.raises(JobFailedError):
            client.run("config", "show", "--invalid")
        failed = client.run("config", "show", "--invalid", check=False)
        assert failed.exit_code == 2
        with pytest.raises(ApiError) as missing:
            client.get_job("not-a-job")
        assert missing.value.status_code == 404
    with Client(rest_url, token="wrong") as unauthenticated:
        with pytest.raises(ApiError) as denied:
            unauthenticated.health()
        assert denied.value.status_code == 401
