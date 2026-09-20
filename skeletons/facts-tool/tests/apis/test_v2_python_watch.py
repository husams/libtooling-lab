"""Automatic registration, import, extraction, and readiness through public SDK."""

import subprocess
import sys
from pathlib import Path

from project import create_project

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python/src"))
from facts_tool.rest import Client, NewClone, RepositoryNotReady


def test_python_repository_creation_automatically_imports_and_indexes(
    server_factory,
    compiler,
    tmp_path,
):
    root = tmp_path / "sdk-repository"
    create_project(root, compiler)
    subprocess.run(["git", "init", "-q", str(root)], check=True)
    (root / "broken.hpp").symlink_to("missing.hpp")
    server = server_factory("--watch", "--debounce-ms", "50")
    with Client(f"http://127.0.0.1:{server.api.port}") as api:
        repo = api.repositories.create(
            name="sdk-repository", clones=[NewClone(path=str(root), label="main")]
        )
        try:
            repo = api.repositories.wait_until_ready(repo.id, timeout=60)
        except RepositoryNotReady as error:
            jobs = [
                api.legacy.get_job(identifier)
                for identifier in api.watcher.status().latest_jobs
            ]
            raise AssertionError(jobs) from error
        assert repo.source_count == repo.indexed_source_count == 1
        symbols = api.symbols.find("ans").collect()
        assert any(symbol.qualified_name == "answer" for symbol in symbols)
        watch = api.watcher.status()
        assert watch.ready
        assert any(w.code == "broken_symlink" for w in watch.warnings)
        assert api.server.readiness().ready
