import httpx
import pytest

from facts_tool.rest import ApiError, Client, ProtocolError, TransportError


@pytest.mark.parametrize("status", [301, 307, 400, 401, 403, 404, 429, 503])
def test_http_errors_are_not_retried_or_redirected(status):
    requests = []

    def handle(request):
        requests.append(request)
        return httpx.Response(status, json={"error": "Rejected"},
                              headers={"Location": "https://another.example/"})

    with (
        Client("http://localhost:1234", token="private-token",
               transport=httpx.MockTransport(handle)) as api,
        pytest.raises(ApiError) as caught,
    ):
        api.submit("import", "--help")
    assert caught.value.status_code == status
    assert "Rejected" in str(caught.value)
    assert "private-token" not in str(caught.value)
    assert len(requests) == 1


def test_non_json_http_failure_remains_an_http_error():
    transport = httpx.MockTransport(lambda _: httpx.Response(502, text="Bad gateway"))
    with (
        Client("http://localhost:1234", transport=transport) as api,
        pytest.raises(ApiError) as caught,
    ):
        api.health()
    assert caught.value.status_code == 502


@pytest.mark.parametrize("error", [httpx.ConnectError, httpx.ReadTimeout])
def test_network_failures_have_a_distinct_error_and_no_retry(error):
    requests = []

    def handle(request):
        requests.append(request)
        raise error("network unavailable", request=request)

    with (
        Client("http://localhost:1234", transport=httpx.MockTransport(handle)) as api,
        pytest.raises(TransportError),
    ):
        api.submit("extract")
    assert len(requests) == 1


@pytest.mark.parametrize("body", [b"broken JSON", b"[]", b"null", b'"ok"', b"42"])
def test_successful_http_response_must_be_a_json_object(body):
    transport = httpx.MockTransport(lambda _: httpx.Response(200, content=body))
    with (
        Client("http://localhost:1234", transport=transport) as api,
        pytest.raises(ProtocolError),
    ):
        api.health()


@pytest.mark.parametrize("method,body", [
    ("commands", {}), ("commands", {"commands": "wrong"}),
    ("commands", {"commands": ["symbol/list"]}),
    ("list_jobs", {}), ("list_jobs", {"jobs": {}}),
    ("list_jobs", {"jobs": [None]}),
])
def test_list_endpoints_reject_malformed_containers(method, body):
    transport = httpx.MockTransport(lambda _: httpx.Response(200, json=body))
    with (
        Client("http://localhost:1234", transport=transport) as api,
        pytest.raises(ProtocolError),
    ):
        getattr(api, method)()
