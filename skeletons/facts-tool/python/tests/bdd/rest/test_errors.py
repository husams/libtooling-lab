import pytest
from pytest_bdd import scenarios, then, when

from facts_tool.rest import ApiError, JobFailedError, TransportError

scenarios("errors.feature")


@when("I request health without valid authentication")
def unauthorized_request(sdk, world):
    with pytest.raises(ApiError) as raised:
        sdk["call"]("health")
    world["error"] = raised.value


@then("the SDK reports HTTP 401 with the server error")
def authentication_error(world):
    assert world["error"].status_code == 401
    assert world["error"].message


@when("I run an invalid CLI command with error checking")
def failed_command(sdk, world):
    with pytest.raises(JobFailedError) as raised:
        sdk["call"]("run", "config", "show", "--invalid", timeout=10)
    world["error"] = raised.value


@then("JobFailedError preserves the exit code and native usage error")
def failure_details(world):
    error = world["error"]
    assert error.job_id == error.job.id
    assert error.job.state == "failed" and error.job.exit_code == 2
    assert "usage error" in error.job.stderr


@when("I repeat the invalid command with error checking disabled")
def unchecked_failure(sdk, world):
    world["failed"] = sdk["call"](
        "run",
        "config",
        "show",
        "--invalid",
        check=False,
        timeout=10,
    )


@then("the SDK returns the failed job without raising")
def failed_snapshot(world):
    assert world["failed"].state == "failed" and world["failed"].exit_code == 2
    assert world["failed"].done and not world["failed"].succeeded


@when("I request a job which does not exist")
def missing_job(sdk, world):
    with pytest.raises(ApiError) as raised:
        sdk["call"]("get_job", "unknown-job")
    world["error"] = raised.value


@then("the SDK reports HTTP 404")
def not_found(world):
    assert world["error"].status_code == 404


@when("I stop the server through the SDK")
def shutdown_server(sdk, world):
    world["shutdown"] = sdk["call"]("shutdown")


@then("shutdown is acknowledged and the HTTP listener closes")
def shutdown_completed(sdk, world):
    import time

    assert world["shutdown"]["status"] == "stopping"
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        try:
            sdk["call"]("health")
        except TransportError:
            return
        time.sleep(0.01)
    raise AssertionError("server kept accepting requests after shutdown")
