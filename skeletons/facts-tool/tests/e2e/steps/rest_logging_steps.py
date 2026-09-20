"""Deployment scenarios use the real async server and its persisted settings."""
import yaml
from pytest_bdd import given, parsers, then, when
from support.rest_http import eventually
from support.rest_logging import events, records


@given(parsers.parse('a native REST {mode} with a separate log file'))
def configured(rest_server, mode):
    assert mode in {"server", "daemon"}
    rest_server.application_log = rest_server.root / "events.jsonl"
    rest_server.config.write_text("logging:\n  file: events.jsonl\n  level: debug\n")
    rest_server.start(["--daemon"] if mode == "daemon" else [])


@given(parsers.parse('a native REST server with verbosity {verbosity:d}'))
def verbose(rest_server, verbosity):
    rest_server.application_log = rest_server.root / "verbose.jsonl"
    rest_server.start(["--log-file", rest_server.application_log, "-v", str(verbosity)])


@when("I complete a successful and a failed CLI job through REST")
def jobs(rest_server):
    rest_server.success = rest_server.api.run(["config", "show"])
    rest_server.failure = rest_server.api.wait(
        rest_server.api.submit(["config", "show", "--invalid-option"]))
    assert rest_server.failure["state"] == "failed"


@then("the separate log reports readiness and the complete job lifecycle")
def lifecycle(rest_server):
    path = rest_server.application_log
    eventually(lambda: {"server.ready", "job.accepted", "job.started", "job.completed"}
               <= events(path))
    output = records(path)
    for job in (rest_server.success, rest_server.failure):
        job_events = [entry for entry in output
                      if entry["fields"].get("job_id") == job["id"]]
        assert [entry["event"] for entry in job_events] == [
            "job.accepted", "job.started", "job.completed"]
        assert job_events[-1]["fields"]["state"] == job["state"]
        assert job_events[-1]["fields"]["exit_code"] == job["exit_code"]


@then("the log destination and level are saved separately from server configuration")
def persisted(rest_server):
    config = yaml.safe_load(rest_server.config.read_text())
    assert config["logging"]["file"] == str(rest_server.application_log)
    assert config["logging"]["level"] == "debug"
    assert "event" not in config


@when("I restart the logged server from its saved configuration")
def restart(rest_server):
    rest_server.shutdown()
    rest_server.output.close()
    rest_server.previous_log = rest_server.application_log.read_bytes()
    rest_server.start(reuse=True)


@then("new lifecycle records are appended without losing the previous run")
def append(rest_server):
    eventually(lambda: len([entry for entry in records(rest_server.application_log)
                           if entry["event"] == "server.ready"]) == 2)
    assert rest_server.application_log.read_bytes().startswith(rest_server.previous_log)


@then(parsers.parse('only events at "{level}" or above are recorded'))
def filtered(rest_server, level):
    rest_server.shutdown()
    output = records(rest_server.application_log)
    order = ["error", "warning", "info", "debug", "trace"]
    allowed = set(order[:order.index(level) + 1])
    assert output and {entry["level"] for entry in output} <= allowed
    assert any(entry["level"] == "error" for entry in output)
    expected = {"info": "server.ready", "debug": "http.response", "trace": "http.read"}
    if level in expected:
        assert expected[level] in events(rest_server.application_log)
    saved = yaml.safe_load(rest_server.config.read_text())["logging"]
    assert saved["level"] == level and "verbosity" not in saved
