"""Public HTTP requests and bounded observation of a real native server."""
import http.client
import json
import time


def eventually(action, timeout=30):
    deadline, last = time.monotonic() + timeout, None
    while time.monotonic() < deadline:
        try:
            value = action()
            if value:
                return value
        except (OSError, ValueError, AssertionError) as error:
            last = error
        time.sleep(0.025)
    raise AssertionError(f"condition did not become true: {last}")


class RestApi:
    def __init__(self, port, token="bdd-secret"):
        self.port, self.token = port, token

    def request(self, method, path, body=None, token=None):
        status, _, content = self.exchange(method, path, body, token)
        return status, json.loads(content) if content else None

    def exchange(self, method, path, body=None, token=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        bearer = self.token if token is None else token
        headers = {"Content-Type": "application/json"}
        if bearer:
            headers["Authorization"] = f"Bearer {bearer}"
        try:
            connection.request(method, path, json.dumps(body) if body else None, headers)
            response = connection.getresponse()
            content = response.read()
            return response.status, dict(response.getheaders()), content.decode()
        finally:
            connection.close()

    def submit(self, arguments, command=None):
        route = f"/v1/commands/{command}" if command else "/v1/jobs"
        status, job = self.request("POST", route, {"arguments": arguments})
        assert status == 202, job
        return job["id"]

    def job(self, identifier):
        status, job = self.request("GET", f"/v1/jobs/{identifier}")
        assert status == 200, job
        return job

    def wait(self, identifier):
        def finished():
            job = self.job(identifier)
            return job if job["state"] in {"succeeded", "failed", "cancelled"} else None
        return eventually(finished)

    def run(self, arguments, command=None):
        job = self.wait(self.submit(arguments, command))
        assert job["state"] == "succeeded" and job["exit_code"] == 0, job
        return job
