import json

import httpx
import pytest

from facts_tool.rest import JobTimeoutError, ProtocolError
from facts_tool.rest.v2.analysis_models import ExtractionResult
from facts_tool.rest.v2.file_analyses import Extractions, Matches
from facts_tool.rest.v2.job_state import JobFailed
from facts_tool.rest.v2.match_models import MatcherBindings
from facts_tool.rest.v2.selections import FileReference, FileSelection

from .v2_helpers import EXTRACTION, job


def test_job_wait_returns_typed_output_and_posts_structured_selection():
    requests = []

    def handle(request):
        requests.append(request)
        body = (
            job() if request.method == "POST" else job("succeeded", result=EXTRACTION)
        )
        return httpx.Response(202 if request.method == "POST" else 200, json=body)

    with httpx.Client(
        base_url="http://test", transport=httpx.MockTransport(handle)
    ) as http:
        handle = Extractions(http).create(
            selection=FileSelection([FileReference("widget.cpp", repository="example")])
        )
        result = handle.wait()
        assert isinstance(result, ExtractionResult) and result.symbols_written == 3
        assert result.files[0].path == "widget.cpp"
    assert requests[0].url.path == "/api/v2/extract/job"
    assert json.loads(requests[0].content) == {
        "selection": {
            "type": "files",
            "files": [{"path": "widget.cpp", "repository": "example"}],
        },
        "force": False,
    }
    assert requests[1].method == "GET"


def test_cancel_uses_delete_and_timeout_never_cancels_remote_work():
    requests = []

    def handle(request):
        requests.append(request)
        return httpx.Response(
            200, json=job("cancelled" if request.method == "DELETE" else "running")
        )

    with httpx.Client(
        base_url="http://test", transport=httpx.MockTransport(handle)
    ) as http:
        handle = Extractions(http).get("job-1")
        with pytest.raises(JobTimeoutError):
            handle.wait(timeout=0)
        assert len(requests) == 1
        assert handle.cancel().state == "cancelled"
        with pytest.raises(JobFailed, match="cancelled"):
            handle.wait()
    assert [r.method for r in requests] == ["GET", "DELETE"]


def test_malformed_typed_result_is_rejected():
    value = {**EXTRACTION, "symbols_written": "three"}
    transport = httpx.MockTransport(
        lambda _: httpx.Response(200, json=job("succeeded", result=value))
    )
    with (
        httpx.Client(base_url="http://test", transport=transport) as http,
        pytest.raises(ProtocolError),
    ):
        Extractions(http).get("job-1")


def test_custom_match_bindings_preserved_without_cli_translation():
    requests = []

    def handle(request):
        requests.append(request)
        return httpx.Response(202, json=job(operation="match"))

    with httpx.Client(
        base_url="http://test", transport=httpx.MockTransport(handle)
    ) as http:
        Matches(http).create(
            selection=FileSelection([FileReference("widget.cpp")]),
            expression='functionDecl().bind("myMethod")',
            bindings=MatcherBindings(source="myMethod"),
        )
    body = json.loads(requests[0].content)
    assert body["expression"] == 'functionDecl().bind("myMethod")'
    assert body["bindings"] == {"source": "myMethod"}
    assert "arguments" not in body


@pytest.mark.parametrize("from_handle", [True, False])
def test_retry_creates_a_new_typed_job_and_preserves_original_handle(from_handle):
    requests = []

    def handle(request):
        requests.append(request)
        body = job("failed") if request.method == "GET" else {
            **job(), "id": "job-2", "retry_of": "job-1",
        }
        return httpx.Response(200 if request.method == "GET" else 202, json=body)

    with httpx.Client(
        base_url="http://test", transport=httpx.MockTransport(handle)
    ) as http:
        resource = Extractions(http)
        original = resource.get("job-1")
        retried = original.retry() if from_handle else resource.retry(original.id)
        assert retried.id == "job-2" and retried.state == "queued"
        assert retried.metadata.retry_of == original.id
        assert original.id == "job-1" and original.state == "failed"
        assert original.metadata.retry_of is None
    assert requests[-1].method == "POST"
    assert requests[-1].url.path == "/api/v2/extract/job"
    assert json.loads(requests[-1].content) == {"retry_of": "job-1"}
