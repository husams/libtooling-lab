"""CLI/YAML precedence and trace log privacy through real native requests."""
import yaml
from pytest_bdd import given, then, when
from support.rest_logging import events


@given("saved server logging defaults")
def defaults(rest_server):
    rest_server.config.write_text("logging:\n  file: unused.log\n  level: error\n")
    rest_server.application_log = rest_server.root / "override.jsonl"


@when("I start the server with CLI logging overrides")
def override(rest_server):
    rest_server.start(["--log-file", "override.jsonl", "--verbose", "3"])
    assert rest_server.api.run(["config", "show"])["state"] == "succeeded"


@then("the chosen file and verbosity override the YAML and survive restart")
def persisted(rest_server):
    rest_server.shutdown()
    rest_server.output.close()
    saved = yaml.safe_load(rest_server.config.read_text())["logging"]
    assert saved == {"file": str(rest_server.application_log), "level": "trace"}
    assert not (rest_server.root / "unused.log").exists()
    assert "http.read" in events(rest_server.application_log)
    previous = rest_server.application_log.read_bytes()
    rest_server.start(reuse=True)
    rest_server.api.run(["config", "show"])
    rest_server.shutdown()
    assert rest_server.application_log.read_bytes().startswith(previous)
    assert yaml.safe_load(rest_server.config.read_text())["logging"] == saved


@when("I send secrets in REST credentials, arguments, paths, and request bodies")
def secrets(rest_server):
    secret = rest_server.secret = "private-marker-627194"
    job = rest_server.api.wait(rest_server.api.submit(["config", "show", f"--{secret}"]))
    assert secret in job["stderr"]
    assert rest_server.api.request("POST", "/v1/jobs", {secret: secret})[0] == 400
    assert rest_server.api.request("GET", f"/private-{secret}?token={secret}")[0] == 404
    assert rest_server.api.request("GET", "/health", token=secret)[0] == 401


@then("HTTP and job records are present without any secret values")
def private(rest_server):
    rest_server.shutdown()
    content = rest_server.application_log.read_text()
    assert rest_server.secret not in content and "bdd-secret" not in content
    assert {"http.response", "http.read", "http.write", "job.completed"} <= events(
        rest_server.application_log)
