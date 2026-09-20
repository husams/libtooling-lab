import httpx
import pytest

from facts_tool.rest import Client, FileSelector, ProtocolError

from .domain_helpers import domain_record, index_record, symbol_record


@pytest.mark.parametrize("name,args,kwargs", [
    ("find_symbols", ("",), {}), ("find_symbols", ("x",), {"limit": 0}),
    ("find_symbols", ("x",), {"limit": True}),
    ("find_symbols", ("x",), {"limit": 501}),
    ("find_symbols", ("x",), {"cursor": ""}),
    ("find_symbols", ("x",), {"cursor": "x" * 257}),
    ("find_symbols", ("λ" * 2049,), {}),
    ("match", (FileSelector("x.cpp"), "x" * 262145), {}),
    ("extract", ({"path": "x.cpp"},), {}),
    ("extract", (FileSelector(""),), {}),
    ("extract", (FileSelector("x.cpp"),), {"force": 1}),
    ("dependencies", (FileSelector("x.cpp", repo=""),), {}),
    ("match", (FileSelector("x.cpp"), ""), {}),
    ("match", (FileSelector("x.cpp"), "decl()"), {"traversal": "bad"}),
    ("match", (FileSelector("x.cpp"), "decl()"), {"capture_source": 1}),
])
def test_invalid_resource_input_never_sends_http(name, args, kwargs):
    def unexpected(_):
        raise AssertionError("invalid input reached HTTP transport")
    transport = httpx.MockTransport(unexpected)
    with (Client("http://localhost:1234", transport=transport) as api,
          pytest.raises((TypeError, ValueError))):
        getattr(api, name)(*args, **kwargs)


@pytest.mark.parametrize("changes", [
    {"operation": "unknown"}, {"state": "unknown"}, {"result": []},
    {"error": {"code": 1, "message": "bad"}}, {"created_at": True},
])
def test_malformed_domain_job_is_rejected(changes):
    transport = httpx.MockTransport(lambda _: httpx.Response(
        200, json=domain_record(**changes)))
    with (Client("http://localhost:1234", transport=transport) as api,
          pytest.raises(ProtocolError)):
        api.get_job("domain-7")


@pytest.mark.parametrize("method,args,body", [
    ("find_symbols", ("x",), {"items": [symbol_record() | {"file_id": True}],
                              "next_cursor": None}),
    ("find_symbols", ("x",), {"items": [symbol_record()]}),
    ("index_status", (), index_record() | {"pending": "yes"}),
    ("index_status", (), index_record() | {"state": "unknown"}),
    ("index_status", (), index_record() | {"symbols": -1}),
])
def test_malformed_resource_response_is_rejected(method, args, body):
    transport = httpx.MockTransport(lambda _: httpx.Response(200, json=body))
    with (Client("http://localhost:1234", transport=transport) as api,
          pytest.raises(ProtocolError)):
        getattr(api, method)(*args)
