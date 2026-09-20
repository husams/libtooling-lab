"""Protocol errors must stay separate from asynchronous CLI command failures."""
import socket

import pytest


def test_health_and_port_persistence(server):
    status, health = server.api.request("GET", "/health")
    assert status == 200
    assert health
    assert f"port: {server.api.port}" in server.config.read_text()
    assert "host: 127.0.0.1" in server.config.read_text()
    assert server.api.port > 0


@pytest.mark.parametrize("body", [{}, {"arguments": "--help"},
                                 {"arguments": [1]}, {"arguments": [None]}, [],
                                 {"arguments": ["config", "show\u0000"]},
                                 {"arguments": ["config"], "extra": True}])
def test_invalid_job_body(server, body):
    status, error = server.api.request("POST", "/v1/jobs", body)
    assert status == 400, error


@pytest.mark.parametrize("raw", ['{', '{"arguments": ["config",]}', 'null'])
def test_malformed_json(server, raw):
    status, error = server.api.request("POST", "/v1/jobs", raw=raw)
    assert status == 400, error


def test_unknown_routes_and_job(server):
    assert server.api.request("GET", "/does-not-exist")[0] == 404
    assert server.api.request("GET", "/v1/jobs/does-not-exist")[0] == 404
    unknown = {"arguments": []}
    assert server.api.request("POST", "/v1/commands/no-such-command", unknown)[0] == 400


@pytest.mark.parametrize("method,path", [("POST", "/health"),
                                         ("DELETE", "/v1/jobs"),
                                         ("GET", "/v1/shutdown")])
def test_wrong_method(server, method, path):
    assert server.api.request(method, path)[0] == 405


def test_authentication(server_factory):
    server = server_factory(token="api-test-secret")
    assert server.api.request("GET", "/v1/jobs", token="wrong")[0] == 401
    assert server.api.request("GET", "/v1/jobs", token="")[0] == 401
    assert server.api.request("GET", "/v1/jobs")[0] == 200
    assert "api-test-secret" not in server.config.read_text()


def test_partial_http_client_does_not_block_health(server):
    with socket.create_connection(("127.0.0.1", server.api.port), timeout=3) as client:
        client.sendall(b"POST /v1/jobs HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n")
        assert server.api.request("GET", "/health")[0] == 200


@pytest.mark.parametrize("message,expected", [
    (b"INVALID REQUEST\r\n\r\n", b"400"),
    (b"POST /v1/jobs HTTP/1.1\r\nHost: localhost\r\n"
     b"Content-Length: 1048577\r\n\r\n", b"413"),
])
def test_malformed_and_oversized_http(server, message, expected):
    with socket.create_connection(("127.0.0.1", server.api.port), timeout=3) as client:
        client.sendall(message)
        assert expected in client.recv(4096).split(b"\r\n", 1)[0]
    assert server.api.request("GET", "/health")[0] == 200


@pytest.mark.parametrize("headers", [{"Host": "foreign.example"},
                                    {"Sec-Fetch-Site": "cross-site"},
                                    {"Origin": "https://foreign.example"}])
def test_browser_requests_are_rejected(server, headers):
    assert server.api.request("GET", "/health", extra_headers=headers)[0] == 403
