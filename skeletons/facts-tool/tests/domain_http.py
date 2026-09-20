"""Domain-only HTTP helpers: requests contain identities, never database paths."""
import time
from urllib.parse import urlencode


def eventually(action, timeout=30):
    deadline, last = time.monotonic() + timeout, None
    while time.monotonic() < deadline:
        try:
            result = action()
            if result:
                return result
        except (OSError, ValueError, AssertionError) as error:
            last = error
        time.sleep(0.025)
    raise AssertionError(f"domain API did not converge: {last}")


def index_ready(api):
    def ready():
        status, state = api.request("GET", "/v1/index")
        assert status == 200, state
        assert state["state"] != "failed", state
        return state if state["state"] == "ready" and not state["pending"] else None
    return eventually(ready)


def symbols(api, qualified_name, **filters):
    parameters = {"qualified_name": qualified_name, **filters}
    status, result = api.request("GET", "/v1/symbols?" + urlencode(parameters))
    assert status == 200, result
    assert isinstance(result["items"], list), result
    assert "next_cursor" in result, result
    return result


def submit(api, endpoint, selector, **fields):
    status, job = api.request("POST", endpoint, {"file": selector, **fields})
    assert status == 202, job
    assert job["id"] and job["operation"], job
    assert "arguments" not in job and "stdout" not in job, job
    return job


def completed(api, job):
    result = api.wait(job["id"])
    assert result["state"] == "succeeded", result
    assert isinstance(result["result"], dict) and result["error"] is None, result
    assert "arguments" not in result and "stdout" not in result, result
    return result


def await_symbols(api, qualified_name, count=1, **filters):
    def present():
        page = symbols(api, qualified_name, **filters)
        return page if len(page["items"]) == count else None
    return eventually(present)
