import httpx
import pytest

from facts_tool.rest import Client, ProtocolError

from .v2_helpers import EXTRACTION, SYMBOL, job


def test_repeated_cursor_is_rejected_instead_of_looping_forever():
    transport = httpx.MockTransport(
        lambda _: httpx.Response(
            200, json={"items": [SYMBOL], "next_cursor": "repeated"}
        )
    )
    with (
        Client("http://test", transport=transport) as api,
        pytest.raises(ProtocolError, match="repeated"),
    ):
        api.symbols.find("Widget").collect()


def test_job_list_metadata_fetches_result_on_wait():
    requests = []

    def handle(request):
        requests.append(request)
        body = {"items": [job("succeeded")], "next_cursor": None}
        if request.url.path.endswith("job-1"):
            body = job("succeeded", result=EXTRACTION)
        return httpx.Response(200, json=body)

    with Client("http://test", transport=httpx.MockTransport(handle)) as api:
        jobs = api.extractions.list()
        assert requests == []
        extraction = next(iter(jobs))
        assert extraction.result is None
        assert extraction.wait().symbols_written == 3
    assert len(requests) == 2


def test_malformed_array_item_fails_before_yielding_untyped_data():
    transport = httpx.MockTransport(
        lambda _: httpx.Response(
            200,
            json={
                "items": [{**SYMBOL, "definition": {"file_id": 1}}],
                "next_cursor": None,
            },
        )
    )
    with (
        Client("http://test", transport=transport) as api,
        pytest.raises(ProtocolError),
    ):
        api.symbols.find("Widget").collect()


def test_wait_reads_summary_without_fetching_result_rows():
    requests = []
    summary = {
        key: value
        for key, value in EXTRACTION.items()
        if key not in {"files", "diagnostics"}
    }

    def handle(request):
        requests.append(request)
        return httpx.Response(200, json=job("succeeded", result=summary))

    with Client("http://test", transport=httpx.MockTransport(handle)) as api:
        result = api.extractions.get("job-1").wait()
        assert result.symbols_written == 3
        assert result.files is None and result.diagnostics is None
    assert len(requests) == 1
    assert requests[0].url.path == "/api/v2/extract/job/job-1"
