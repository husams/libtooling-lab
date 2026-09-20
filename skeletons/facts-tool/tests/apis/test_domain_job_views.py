"""Job polling returns results while list views remain small under native work."""
import time
from concurrent.futures import ThreadPoolExecutor

from domain_http import completed, submit


def test_completed_job_detail_and_cancel_preserve_full_result(domain_server):
    api = domain_server.api
    job = submit(api, "/v1/dependencies", {"path": "src/main.cpp", "repo": "alpha"})
    done = completed(api, job)
    status, listing = api.request("GET", "/v1/jobs")
    assert status == 200
    metadata, = [item for item in listing["jobs"] if item["id"] == job["id"]]
    assert metadata["state"] == "succeeded" and metadata["result"] is None
    status, cancelled = api.request("DELETE", f"/v1/jobs/{job['id']}")
    assert status == 200 and cancelled == done
    assert api.job(job["id"]) == done


def test_large_match_result_polling_preserves_health_responsiveness(domain_server, domain_project):
    source = domain_project.sources["alpha"]
    count = 2500
    source.write_text("namespace volume {\n" + "".join(
        f"int function_{index}() {{ return {index}; }}\n" for index in range(count)) + "}\n")
    api = domain_server.api
    job = submit(api, "/v1/matches", {"path": str(source)},
                 query='functionDecl(isDefinition()).bind("selected")')
    done = completed(api, job)
    assert done["result"]["match_count"] == count
    with ThreadPoolExecutor(max_workers=4) as pool:
        readers = [pool.submit(api.job, job["id"]) for _ in range(4)]
        for _ in range(5):
            started = time.monotonic()
            assert api.request("GET", "/health")[0] == 200
            assert time.monotonic() - started < 2
        assert all(reader.result(timeout=10) == done for reader in readers)
    status, _, listing = api.exchange("GET", "/v1/jobs")
    assert status == 200 and len(listing) < 4096
