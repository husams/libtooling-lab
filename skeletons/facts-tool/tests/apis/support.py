"""Small HTTP client shared by the native server integration tests."""
import http.client
import json
import time


def eventually(action, timeout=30):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        try:
            value = action()
            if value:
                return value
        except (OSError, ValueError, AssertionError) as error:
            last = error
        time.sleep(0.05)
    raise AssertionError(f"condition did not become true: {last}")


class Api:
    def __init__(self, port, token=None):
        self.port = port
        self.token = token

    def request(self, method, path, body=None, raw=None, token=None, extra_headers=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        headers = {"Content-Type": "application/json"}
        headers.update(extra_headers or {})
        bearer = self.token if token is None else token
        if bearer:
            headers["Authorization"] = f"Bearer {bearer}"
        payload = json.dumps(body) if body is not None else raw
        try:
            connection.request(method, path, body=payload, headers=headers)
            response = connection.getresponse()
            content = response.read()
            return response.status, json.loads(content) if content else None
        finally:
            connection.close()

    def submit(self, arguments, command=None):
        endpoint = f"/v1/commands/{command}" if command else "/v1/jobs"
        status, job = self.request("POST", endpoint, {"arguments": arguments})
        assert status == 202, job
        assert job["id"]
        return job["id"]

    def job(self, identifier):
        status, job = self.request("GET", f"/v1/jobs/{identifier}")
        assert status == 200, job
        return job

    def wait(self, identifier, timeout=30):
        def finished():
            job = self.job(identifier)
            return job if job["state"] in {"succeeded", "failed", "cancelled"} else None
        return eventually(finished, timeout)

    def run(self, arguments, command=None):
        job = self.wait(self.submit(arguments, command))
        assert job["state"] == "succeeded", job
        assert job["exit_code"] == 0, job
        return job
