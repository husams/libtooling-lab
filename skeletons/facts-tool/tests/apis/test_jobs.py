"""Exercise genuine command processes, concurrent clients and failure results."""
from concurrent.futures import ThreadPoolExecutor


def test_generic_job_and_output(server):
    job = server.api.run(["config", "show"])
    assert "conf_root" in job["stdout"]
    assert isinstance(job["stderr"], str)
    assert job["truncated"] is False
    assert job["timed_out"] is False


def test_invalid_cli_option_is_a_failed_job(server):
    job = server.api.wait(server.api.submit(["config", "show", "--invalid-option"]))
    assert job["state"] == "failed"
    assert job["exit_code"] == 2
    assert "usage error" in job["stderr"]


def test_concurrent_clients_and_responsive_health(server):
    with ThreadPoolExecutor(max_workers=8) as pool:
        ids = list(pool.map(lambda _: server.api.submit(["config", "show"]), range(16)))
        assert server.api.request("GET", "/health")[0] == 200
        jobs = list(pool.map(server.api.wait, ids))
    assert len(set(ids)) == 16
    assert all(job["exit_code"] == 0 for job in jobs)
    ordered = sorted(jobs, key=lambda job: job["started_at"])
    assert all(left["finished_at"] <= right["started_at"]
               for left, right in zip(ordered, ordered[1:]))
    status, listing = server.api.request("GET", "/v1/jobs")
    assert status == 200
    listed = listing["jobs"] if isinstance(listing, dict) else listing
    assert set(ids).issubset({job["id"] for job in listed})


def test_server_lifecycle_cannot_be_enqueued(server):
    status, error = server.api.request("POST", "/v1/jobs", {"arguments": ["serve"]})
    assert status == 400, error


def test_shell_metacharacters_are_literal_arguments(server, tmp_path):
    marker = tmp_path / "must-not-exist"
    command = f"$(touch {marker})"
    job = server.api.wait(server.api.submit(["config", "show", command]))
    assert job["state"] == "failed"
    assert not marker.exists()
