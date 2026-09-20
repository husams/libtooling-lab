import pytest
from pytest_bdd import scenarios, then, when

from facts_tool.rest import JobTimeoutError

from .project_steps import *  # noqa: F403

scenarios("job_control.feature")


@when("I cancel a configuration job queued behind the blocked import")
def cancel_queued(sdk, world):
    job = sdk["call"]("submit", "config", "show")
    assert job.state == "queued"
    world["cancelled"] = sdk["call"]("cancel_job", job.id)


@then("the cancelled job never starts and the active import remains running")
def queued_cancelled(sdk, world):
    cancelled = sdk["call"]("get_job", world["cancelled"].id)
    assert cancelled.state == "cancelled" and cancelled.done
    assert cancelled.started_at is None and cancelled.stdout == ""
    assert sdk["call"]("get_job", world["blocked"].id).state == "running"


@when("I cancel the running import")
def cancel_running(sdk, world):
    world["cancelled"] = sdk["call"]("cancel_job", world["blocked"].id)


@then("the import becomes cancelled and the next command can complete")
def running_cancelled(sdk, world):
    job = sdk["call"]("wait", world["blocked"].id, timeout=5)
    assert job.state == "cancelled" and job.done and not job.succeeded
    world["lock"].rollback()
    assert sdk["call"]("run", "config", "show", timeout=5).succeeded


@when("my deadline expires while polling the running import")
def polling_timeout(sdk, world):
    with pytest.raises(JobTimeoutError) as raised:
        sdk["call"]("wait", world["blocked"].id, timeout=0.05, poll_interval=0.01)
    world["timeout"] = raised.value


@then("the timeout identifies the job and does not cancel remote work")
def remote_still_running(sdk, world):
    assert world["timeout"].job_id == world["blocked"].id
    assert world["timeout"].timeout == 0.05
    assert sdk["call"]("get_job", world["blocked"].id).state == "running"
    assert sdk["call"]("health")["status"] == "ok"


@when("the other connection releases the database lock")
def unlock_project(world):
    world["lock"].rollback()


@then("the original import completes successfully")
def import_finishes(sdk, world):
    job = sdk["call"]("wait", world["blocked"].id, timeout=5)
    assert job.succeeded and job.exit_code == 0 and not job.timed_out, job.stderr
