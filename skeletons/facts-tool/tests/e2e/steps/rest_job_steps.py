"""Observable command output and concurrent HTTP clients using native jobs."""
from concurrent.futures import ThreadPoolExecutor
from itertools import pairwise

from pytest_bdd import parsers, then, when


@when(parsers.parse('I submit a REST CLI command with "{outcome}" arguments'))
def submit_command(rest_server, outcome):
    arguments = ["config", "show"] + (["--invalid-option"] if outcome == "invalid" else [])
    rest_server.job_id = rest_server.api.submit(arguments)
    rest_server.result = rest_server.api.wait(rest_server.job_id)


@then(parsers.parse('the REST job reports "{state}" with exit code {code:d} and "{text}"'))
def result(rest_server, state, code, text):
    job = rest_server.result
    assert job["state"] == state and job["exit_code"] == code, job
    assert text in job["stdout"] + job["stderr"]
    assert isinstance(job["stdout"], str) and isinstance(job["stderr"], str)
    assert not job["truncated"] and not job["timed_out"]
    assert rest_server.api.job(rest_server.job_id) == job


@when("sixteen HTTP clients submit CLI jobs concurrently")
def concurrent_jobs(rest_server):
    with ThreadPoolExecutor(max_workers=8) as pool:
        rest_server.job_ids = list(pool.map(
            lambda _: rest_server.api.submit(["config", "show"]), range(16)))
        assert rest_server.api.request("GET", "/health")[0] == 200
        rest_server.results = list(pool.map(rest_server.api.wait, rest_server.job_ids))


@then("all jobs finish successfully with unique identities and available health")
def concurrent_results(rest_server):
    assert len(set(rest_server.job_ids)) == 16
    assert all(job["state"] == "succeeded" and job["exit_code"] == 0
               for job in rest_server.results)
    status, listing = rest_server.api.request("GET", "/v1/jobs")
    assert status == 200
    listed = listing["jobs"] if isinstance(listing, dict) else listing
    assert set(rest_server.job_ids).issubset({job["id"] for job in listed})
    ordered = sorted(rest_server.results, key=lambda job: job["started_at"])
    assert all(left["finished_at"] <= right["started_at"]
               for left, right in pairwise(ordered))
    assert rest_server.api.request("GET", "/health")[0] == 200


@when("I query symbols, run a matcher, and build a call graph through REST")
def analyse_project(rest_server):
    root = rest_server.project
    paths = ["-c", str(root / "project.db"), "-f", str(root / "facts.db")]
    rest_server.symbols = rest_server.api.run(paths, "symbol/list")
    rest_server.matches = rest_server.api.run(
        [*paths, "--matcher", 'functionDecl(hasName("main")).bind("symbol")',
         str(rest_server.source)], "match")
    rest_server.graph = rest_server.api.run([*paths, "--function", "main"], "analyse/call-graph")


@then("the REST results contain extracted functions, the match, and a complete graph")
def project_results(rest_server):
    assert "answer" in rest_server.symbols["stdout"]
    assert "main" in rest_server.symbols["stdout"]
    assert "main" in rest_server.matches["stdout"] + rest_server.matches["stderr"]
    assert "complete" in rest_server.graph["stdout"] + rest_server.graph["stderr"]
    assert (rest_server.project / "project.db").is_file()
    assert (rest_server.project / "facts.db").is_file()
