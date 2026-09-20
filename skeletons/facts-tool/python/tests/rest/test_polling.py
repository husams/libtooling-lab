import httpx
import pytest

from facts_tool.rest import Client, JobFailedError, JobTimeoutError

from .helpers import job_record


def test_wait_returns_only_terminal_job_and_preserves_output():
    states = iter(["queued", "running", "succeeded"])
    requests = []

    def handle(request):
        requests.append(request)
        return httpx.Response(200, json=job_record(next(states), stdout="result\n"))

    with Client("http://localhost:1234", transport=httpx.MockTransport(handle)) as api:
        result = api.wait("42", poll_interval=0.001)
    assert result.succeeded and result.stdout == "result\n"
    assert len(requests) == 3


def test_wait_deadline_reports_job_without_cancelling_server_work():
    methods = []

    def handle(request):
        methods.append(request.method)
        return httpx.Response(200, json=job_record("running"))

    with (
        Client("http://localhost:1234", transport=httpx.MockTransport(handle)) as api,
        pytest.raises(JobTimeoutError) as caught,
    ):
        api.wait("42", timeout=0.005, poll_interval=0.001)
    assert caught.value.job_id == "42" and caught.value.timeout == 0.005
    assert methods and set(methods) == {"GET"}


@pytest.mark.parametrize("state,exit_code", [("failed", 2), ("cancelled", -1)])
def test_run_checks_cli_failure_even_when_http_requests_succeed(state, exit_code):
    requests = []

    def handle(request):
        requests.append(request)
        body = job_record() if request.method == "POST" else job_record(
            state, exit_code=exit_code, stderr="execution failed")
        return httpx.Response(202 if request.method == "POST" else 200, json=body)

    with Client("http://localhost:1234", transport=httpx.MockTransport(handle)) as api:
        with pytest.raises(JobFailedError) as caught:
            api.run("extract", poll_interval=0.001)
        assert caught.value.job.exit_code == exit_code
        assert caught.value.job.stderr == "execution failed"
        result = api.run("extract", check=False, poll_interval=0.001)
    assert result.state == state and result.done
    assert [r.method for r in requests] == ["POST", "GET", "POST", "GET"]


def test_wait_returns_server_timeout_details_for_caller_inspection():
    record = job_record("failed", exit_code=-1, timed_out=True, truncated=True)
    transport = httpx.MockTransport(lambda _: httpx.Response(200, json=record))
    with Client("http://localhost:1234", transport=transport) as api:
        result = api.wait("42")
    assert result.done and result.timed_out and result.truncated


def test_run_success_keeps_captured_text_exact():
    def handle(request):
        body = job_record() if request.method == "POST" else job_record(
            "succeeded", stdout="{\"symbols\": []}\n", stderr="warning\n")
        return httpx.Response(200, json=body)

    with Client("http://localhost:1234", transport=httpx.MockTransport(handle)) as api:
        job = api.run("symbol", "list")
    assert job.stdout == '{"symbols": []}\n' and job.stderr == "warning\n"


def test_zero_wait_budget_expires_without_requesting_or_cancelling():
    def handle(_):
        pytest.fail("zero wait budget must expire before requesting")

    with (
        Client("http://localhost:1234", transport=httpx.MockTransport(handle)) as api,
        pytest.raises(JobTimeoutError),
    ):
        api.wait("42", timeout=0)
