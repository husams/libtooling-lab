"""An explicit global index job reads externally written facts without extraction."""
import sqlite3
from urllib.parse import quote

from test_v2_analysis_jobs import run


def test_manual_cli_extraction_and_noop_index(domain_server, domain_project):
    project, api = domain_project, domain_server.api
    first, _ = run(api, "index", {})
    assert first["result"]["sources_processed"] == 0
    assert first["result"]["sources_skipped"] >= 2
    source = project.sources["alpha"]
    project.write_source(source, "alpha", "manual_update")
    project.cli("extract", "--force", "-o", project.facts["alpha"], source)
    source_mtime = source.stat().st_mtime_ns
    indexed, _ = run(api, "index", {})
    result = indexed["result"]
    assert result["sources_processed"] == 1
    assert result["sources_skipped"] >= 1
    assert result["index_revision"] != first["result"]["index_revision"]
    status, symbols = api.request("GET", "/api/v2/symbols?qualified_name=alpha%3A%3Amanual_update&match=exact")
    assert status == 200 and symbols["items"], symbols
    unchanged, _ = run(api, "index", {})
    assert unchanged["result"]["sources_processed"] == 0
    assert unchanged["result"]["index_revision"] == result["index_revision"]
    assert source.stat().st_mtime_ns == source_mtime


def test_wal_only_update_is_indexed(domain_server, domain_project):
    project, api = domain_project, domain_server.api
    path = project.facts["alpha"]
    with sqlite3.connect(path) as writer:
        writer.execute("PRAGMA journal_mode=WAL")
        writer.execute("PRAGMA wal_autocheckpoint=0")
        writer.execute("PRAGMA wal_checkpoint(TRUNCATE)")
        run(api, "index", {})
        before = path.stat().st_mtime_ns
        writer.execute("UPDATE symbol SET qualified_name='alpha::wal_updated' WHERE qualified_name='alpha::answer'")
        writer.commit()
        assert path.stat().st_mtime_ns == before
        job, _ = run(api, "index", {})
        assert job["result"]["sources_processed"] == 1
        status, found = api.request("GET", "/api/v2/symbols?qualified_name=" + quote("alpha::wal_updated") + "&match=exact")
        assert status == 200 and found["items"], found
