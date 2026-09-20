import httpx
import pytest

from facts_tool.rest import Client


@pytest.mark.parametrize("url", [
    "file:///tmp/api", "localhost:8080", "http://user:secret@localhost",
    "http://localhost?token=secret", "http://localhost#fragment",
    "http://local host", "http://localhost\\other",
])
def test_base_url_rejects_ambiguous_or_non_http_addresses(url):
    with pytest.raises((TypeError, ValueError)):
        Client(url)


@pytest.mark.parametrize("token", ["bad\r\nHeader: value", "bad\nvalue"])
def test_token_rejects_header_injection(token):
    with pytest.raises((TypeError, ValueError)):
        Client("http://localhost:1234", token=token)


@pytest.mark.parametrize("method,args", [
    ("submit", ()), ("submit", ("extract", 7)),
    ("submit", ("extract\0more",)),
    ("command", ("../shutdown",)), ("command", ("symbol/list?all=1",)),
    ("command", ("/v1/shutdown",)), ("command", ("symbol\\list",)),
    ("get_job", ("42/../shutdown",)), ("get_job", ("42?other=1",)),
    ("cancel_job", ("42#fragment",)), ("get_job", ("",)),
])
def test_invalid_request_inputs_fail_before_any_http_request(method, args):
    def handle(_):
        pytest.fail("invalid client input must not issue an HTTP request")

    with (
        Client("http://localhost:1234", transport=httpx.MockTransport(handle)) as api,
        pytest.raises((TypeError, ValueError)),
    ):
        getattr(api, method)(*args)


@pytest.mark.parametrize("option,value", [
    ("poll_interval", 0), ("poll_interval", -1),
    ("poll_interval", float("nan")), ("poll_interval", float("inf")),
    ("timeout", -1), ("timeout", float("nan")), ("timeout", float("inf")),
])
@pytest.mark.parametrize("method", ["wait", "run"])
def test_invalid_poll_settings_fail_before_any_request(option, value, method):
    def handle(_):
        pytest.fail("invalid polling settings must fail before requesting")

    with (
        Client("http://localhost:1234", transport=httpx.MockTransport(handle)) as api,
        pytest.raises((TypeError, ValueError)),
    ):
        getattr(api, method)("42", **{option: value})
