from pytest_bdd import scenarios, then, when

from facts_tool import open_codebase

from .project_steps import *  # noqa: F403
from .project_steps import import_arguments

scenarios("workflow.feature")


@when("I import and extract the project through the SDK")
def import_and_extract(sdk, project, world):
    world["import"] = sdk["call"]("run", *import_arguments(project), timeout=10)
    world["extract"] = sdk["call"](
        "run",
        "extract",
        "-c",
        str(project["db"]),
        "-o",
        str(project["facts"]),
        timeout=10,
    )


@then("both remote commands succeed with captured process results")
def successful_commands(world):
    for name in ("import", "extract"):
        job = world[name]
        assert job.done and job.succeeded and job.exit_code == 0
        assert isinstance(job.stdout, str) and isinstance(job.stderr, str)
        assert not job.timed_out and not job.truncated


@then("the SQLite SDK finds the extracted answer function")
def query_extracted_facts(project):
    with open_codebase(facts_db=project["facts"], project_db=project["db"]) as cb:
        assert {"answer", "main"}.issubset(set(cb.query().nodes().names()))


@when("I invoke the named call graph command and poll its job")
def call_graph(sdk, project, world):
    job = sdk["call"](
        "command",
        "analyse/call-graph",
        "-c",
        str(project["db"]),
        "-f",
        str(project["facts"]),
        "--function",
        "main",
    )
    world["graph"] = sdk["call"]("wait", job.id, timeout=10)


@then("the completed job is discoverable with metadata and detailed output")
def discoverable_graph(sdk, world):
    job = world["graph"]
    assert job.succeeded
    listed = next(item for item in sdk["call"]("list_jobs") if item.id == job.id)
    assert listed.state == "succeeded" and listed.stdout is None
    detailed = sdk["call"]("get_job", job.id)
    assert detailed.arguments == job.arguments
    assert detailed.stdout == job.stdout and detailed.stderr == job.stderr
    assert detailed.created_at <= detailed.started_at <= detailed.finished_at


@when("I request the server health and API capabilities")
def inspect_capabilities(sdk, world):
    world["health"] = sdk["call"]("health")
    world["commands"] = sdk["call"]("commands")
    world["openapi"] = sdk["call"]("openapi")
    world["watch"] = sdk["call"]("watch_status")


@then("the server advertises CLI commands job routes and watch state")
def capabilities(world):
    assert world["health"]["status"] == "ok"
    assert {"import", "extract"}.issubset({c["path"] for c in world["commands"]})
    assert "/v1/jobs" in world["openapi"]["paths"]
    assert world["watch"]["enabled"] is False
