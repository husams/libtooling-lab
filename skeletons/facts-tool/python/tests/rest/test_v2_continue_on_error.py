"""Sync and async clients preserve partial job status and paginate typed failures."""
import asyncio
import json

import httpx
import pytest

from facts_tool.rest import AsyncClient, Client, FileFailure, FileReference, FileSelection
from .v2_helpers import EXTRACTION, job


@pytest.mark.parametrize("asynchronous", [False, True])
@pytest.mark.parametrize("family,resource", [
    ("extract", "extractions"), ("match", "matches"), ("dependencies", "dependencies")])
def test_partial_jobs_and_paginated_failures(asynchronous, family, resource):
    requests = []
    failure = {"file_id": "1", "path": "broken.cpp", "error": {
        "code": "analysis_failed", "message": "missing header",
        "details": {"compilation_commands": [{"arguments": ["-include", "missing.hpp"]}]}}}
    result = {k: v for k, v in EXTRACTION.items() if k not in {"symbols_written", "files"}}
    result.update(files_selected=3, files_processed=1, files_failed=2,
                  files_not_attempted=0, coverage="partial")
    result[{"extract": "symbols_written", "match": "match_count", "dependencies": "edge_count"}[family]] = 3

    def handle(request):
        requests.append(request)
        if request.method == "POST":
            return httpx.Response(202, json=job(operation=family))
        if request.url.path.endswith("/results"):
            assert request.url.params["collection"] == "failed_files"
            second = request.url.params.get("cursor") == "next"
            row = {**failure, "file_id": "2", "path": "also-broken.cpp"} if second else failure
            return httpx.Response(200, json={"items": [row], "next_cursor": None if second else "next"})
        return httpx.Response(200, json=job("succeeded", operation=family, result=result))

    kwargs = {"selection": FileSelection([FileReference("broken.cpp")]), "continue_on_error": True}
    if family == "match":
        kwargs["expression"] = 'functionDecl().bind("f")'

    async def asynchronous_run():
        async with AsyncClient("http://test", transport=httpx.MockTransport(handle)) as api:
            jobs = getattr(api, resource)
            submitted = await jobs.create(**kwargs)
            summary = await submitted.wait()
            failures = await jobs.failed_files(submitted.id, limit=1).collect()
            return summary, failures

    if asynchronous:
        summary, failures = asyncio.run(asynchronous_run())
    else:
        with Client("http://test", transport=httpx.MockTransport(handle)) as api:
            jobs = getattr(api, resource)
            submitted = jobs.create(**kwargs)
            summary = submitted.wait()
            failures = jobs.failed_files(submitted.id, limit=1).collect()
    assert summary.coverage == "partial" and summary.files_failed == 2
    assert summary.files_not_attempted == 0
    assert json.loads(requests[0].content)["continue_on_error"] is True
    assert [f.file_id for f in failures] == ["1", "2"]
    assert all(isinstance(f, FileFailure) for f in failures)
    assert failures[0].error.details == failure["error"]["details"]
