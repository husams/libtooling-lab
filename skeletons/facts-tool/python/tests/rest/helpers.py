import httpx


def job_record(state="queued", **changes):
    """Native REST job schema, including metadata present before execution."""
    record = {
        "id": "42",
        "state": state,
        "arguments": ["symbol", "list"],
        "exit_code": 0 if state == "succeeded" else None,
        "stdout": "",
        "stderr": "",
        "truncated": False,
        "timed_out": False,
        "created_at": 1_750_000_000_000,
    }
    return record | changes


def response_for(request):
    endpoints = {
        "/health": {"status": "ok"},
        "/openapi.json": {"openapi": "3.1.0", "paths": {}},
        "/v1/commands": {
            "commands": [{
                "path": "symbol/list", "endpoint": "/v1/commands/symbol/list",
            }]
        },
        "/v1/watch": {"enabled": True, "backend": "inotify"},
        "/v1/shutdown": {"status": "stopping"},
        "/v1/jobs": {"jobs": [job_record()]},
    }
    if request.url.path == "/v1/jobs" and request.method == "POST":
        return httpx.Response(202, json=job_record())
    if request.url.path.startswith(("/v1/jobs/", "/v1/commands/")):
        return httpx.Response(200, json=job_record())
    return httpx.Response(200, json=endpoints[request.url.path])
