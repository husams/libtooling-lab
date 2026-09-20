from dataclasses import FrozenInstanceError

import httpx
import pytest

from facts_tool.rest import Client, JobFailedError, ProtocolError

from .helpers import job_record


def get_job(record):
    transport = httpx.MockTransport(lambda _: httpx.Response(200, json=record))
    with Client("http://localhost:1234", transport=transport) as client:
        return client.get_job("42")


@pytest.mark.parametrize("state,done,succeeded", [
    ("queued", False, False), ("running", False, False),
    ("succeeded", True, True), ("failed", True, False),
    ("cancelled", True, False),
])
def test_job_preserves_native_state_and_execution_details(state, done, succeeded):
    record = job_record(state, stdout="λ\n", stderr="diagnostic\n", truncated=True,
                        started_at=1_750_000_000_005, finished_at=1_750_000_000_010)
    job = get_job(record)
    assert (job.done, job.succeeded) == (done, succeeded)
    assert job.arguments == ("symbol", "list")
    assert job.stdout == "λ\n" and job.stderr == "diagnostic\n"
    assert job.truncated and not job.timed_out
    assert job.created_at < job.started_at < job.finished_at
    with pytest.raises((FrozenInstanceError, AttributeError)):
        job.state = "modified"


def test_list_metadata_distinguishes_unfetched_output_from_empty_output():
    record = job_record()
    del record["stdout"], record["stderr"]
    transport = httpx.MockTransport(
        lambda _: httpx.Response(200, json={"jobs": [record]}))
    with Client("http://localhost:1234", transport=transport) as client:
        job = client.list_jobs()[0]
    assert job.stdout is None and job.stderr is None
    assert job.started_at is None and job.finished_at is None


@pytest.mark.parametrize("field,value", [
    ("id", 42), ("state", "unknown"), ("arguments", "extract"),
    ("arguments", [1]), ("exit_code", True), ("stdout", 7),
    ("stderr", []), ("truncated", "false"), ("timed_out", 0),
    ("created_at", True), ("started_at", "yesterday"), ("finished_at", 1.5),
])
def test_malformed_job_fields_raise_protocol_errors(field, value):
    with pytest.raises(ProtocolError):
        get_job(job_record(**{field: value}))


@pytest.mark.parametrize("field", [
    "id", "state", "arguments", "exit_code", "truncated", "timed_out", "created_at",
])
def test_missing_required_job_fields_raise_protocol_errors(field):
    record = job_record()
    del record[field]
    with pytest.raises(ProtocolError):
        get_job(record)


def test_job_failure_exposes_complete_job_result():
    job = get_job(job_record("failed", exit_code=2, stderr="invalid matcher"))
    with pytest.raises(JobFailedError) as caught:
        job.raise_for_status()
    assert caught.value.job is job and caught.value.job_id == "42"
    assert get_job(job_record("succeeded")).raise_for_status().succeeded
