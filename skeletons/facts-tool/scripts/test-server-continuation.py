#!/usr/bin/env python3
"""Exercise failure collection against real facts-tool sources; restore commands afterward."""
import argparse
import asyncio
from dataclasses import asdict, replace
from contextlib import contextmanager
import hashlib
import json
from pathlib import Path
import subprocess
import time

import yaml
from facts_tool.rest import (
    AsyncClient, Client, FileIdentity, FileSelection, JobFailed, RepositorySelection,
    WatcherSettings,
)

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / ".server-runtime"


@contextmanager
def paused_watcher(api, previous):
    api.watcher.update_settings(enabled=False)
    try:
        yield
    finally:
        api.watcher.replace_settings(previous)


def cli_added_facts(api, source, evidence, save):
    """Publish a new CLI-created database for a real source, then restore its association."""
    original = RUNTIME / "facts/src/config/ConfigurationSearch.db"
    additional = evidence / "cli-added-facts.db"
    assert original.exists()
    common = [str(RUNTIME / "build/facts-tool"), "extract", "--force", "--conf",
              str(RUNTIME / "project.db"), "--config", str(RUNTIME / "defaults.yaml")]

    def extract(destination, label):
        command = [*common, "-o", str(destination), source]
        process = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
        save(label, {"command": command, "exit_code": process.returncode,
                     "stdout": process.stdout, "stderr": process.stderr})
        assert process.returncode == 0

    before = api.index.create().wait(timeout=180)
    try:
        extract(additional, "cli-added-extraction")
        assert additional.exists()
        job = api.index.create()
        indexed = job.wait(timeout=180)
        save("rest-reindex-added", {"before": asdict(before), "metadata": asdict(job.metadata),
                                    "result": asdict(indexed)})
        assert indexed.sources_processed >= 1
        assert api.symbols.find("facts::config::resolve", repository="facts-tool", match="exact").collect()
        unchanged = api.index.create().wait(timeout=180)
        assert unchanged.sources_processed == 0 and unchanged.index_revision == indexed.index_revision
        save("no-change-reindex", asdict(unchanged))
        return job.id
    finally:
        extract(original, "cli-restored-association")
        restored = api.index.create()
        save("restored-index", {"id": restored.id, "result": asdict(restored.wait(timeout=180))})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--restore-watcher", type=Path,
                        help="Restore these saved settings instead of the current settings")
    args = parser.parse_args()
    config = yaml.safe_load((RUNTIME / "server.yaml").read_text())
    url = f"http://{config['host']}:{config['port']}"
    evidence = RUNTIME / "evidence" / ("continuation-live-" + time.strftime("%Y%m%dT%H%M%S"))
    evidence.mkdir(parents=True)
    print(f"Evidence: {evidence}", flush=True)

    def save(name, value):
        (evidence / f"{name}.json").write_text(json.dumps(value, indent=2, default=str) + "\n")

    def retained(resource, job, name):
        bundle = {"metadata": asdict(job.metadata), "result": asdict(job.result) if job.result else None}
        if job.result:
            bundle["failed_files"] = [asdict(f) for f in resource.failed_files(job.id, limit=1)]
            bundle["diagnostics"] = [asdict(d) for d in resource.diagnostics(job.id)]
            bundle["rows"] = [asdict(r) for r in resource.results(job.id)]
        save(name, bundle)
        print(f"{name}: {job.id} {job.state} {bundle['result']}", flush=True)
        return bundle

    with Client(url, timeout=60) as api:
        previous = (WatcherSettings(**json.loads(args.restore_watcher.read_text()))
                    if args.restore_watcher else api.watcher.settings())
        with paused_watcher(api, previous):
            names = ["ConfigurationMerge.cpp", "ConfigurationPaths.cpp", "ConfigurationSearch.cpp"]
            catalog = {f.name: f for f in api.files.list(repository="facts-tool") if f.name in names}
            files = [catalog[name] for name in names]
            assert all(f.compilation_command for f in files)
            save("original-files", [asdict(f) for f in files])
            original_hashes = {f.path: hashlib.sha256(Path(f.path).read_bytes()).hexdigest() for f in files}
            selection = FileSelection([FileIdentity(f.id) for f in files])
            submitted = []
            try:
                for file in files[:2]:
                    command = replace(file.compilation_command, arguments=[
                        *file.compilation_command.arguments, "-include", str(evidence / "deliberately-missing.hpp")])
                    api.files.update(file.id, compilation_command=command)
                stopped = api.extractions.create(selection=selection, force=True)
                try:
                    stopped.wait(timeout=180)
                    raise AssertionError("Default extraction should stop")
                except JobFailed:
                    pass
                retained(api.extractions, stopped, "default-stop")
                assert stopped.result.files_failed == 1 and stopped.result.files_not_attempted == 2
                for family, resource, options in [
                    ("extract", api.extractions, {"force": True}),
                    ("match", api.matches, {"expression": 'functionDecl(hasName("facts::config::resolve")).bind("f")'}),
                    ("dependencies", api.dependencies, {}),
                ]:
                    job = resource.create(selection=selection, continue_on_error=True, **options)
                    result = job.wait(timeout=180)
                    bundle = retained(resource, job, family)
                    assert (result.files_selected, result.files_processed, result.files_failed) == (3, 1, 2)
                    assert result.files_not_attempted == 0 and result.coverage == "partial"
                    assert len(bundle["failed_files"]) == 2
                    assert [f["file_id"] for f in bundle["failed_files"]] == [f.id for f in files[:2]]
                    assert all(f["error"]["details"]["effective_compilation_commands"] for f in bundle["failed_files"])
                    submitted.append((family, resource, job))
                all_failed = api.extractions.create(selection=FileSelection([FileIdentity(f.id) for f in files[:2]]),
                                                    force=True, continue_on_error=True)
                try:
                    all_failed.wait(timeout=180)
                    raise AssertionError("A batch where every file failed must be failed")
                except JobFailed:
                    assert all_failed.state == "failed"
                result = all_failed.result
                retained(api.extractions, all_failed, "all-files-failed")
                assert result.files_failed == 2 and result.files_processed == result.files_not_attempted == 0

                async def async_check():
                    async with AsyncClient(url, timeout=60) as async_api:
                        job = await async_api.extractions.create(selection=selection, force=True, continue_on_error=True)
                        result = await job.wait(timeout=180)
                        failed = await async_api.extractions.failed_files(job.id, limit=1).collect()
                        save("async-extract", {"metadata": asdict(job.metadata), "result": asdict(result),
                                               "failed_files": [asdict(f) for f in failed]})
                        assert result.files_processed == 1 and len(failed) == result.files_failed == 2
                asyncio.run(async_check())

                cancelled = api.extractions.create(selection=RepositorySelection("facts-tool"),
                                                   continue_on_error=True)
                deadline = time.monotonic() + 30
                while cancelled.refresh().state == "queued":
                    assert time.monotonic() < deadline
                    time.sleep(0.01)
                assert cancelled.state == "running"
                cancelled.cancel()
                try:
                    cancelled.wait(timeout=180)
                    raise AssertionError("Cancellation must stop continuation")
                except JobFailed:
                    assert cancelled.state == "cancelled"
                retained(api.extractions, cancelled, "cancelled")
                if cancelled.result:
                    result = cancelled.result
                    assert result.files_selected == sum((result.files_processed, result.files_skipped,
                                                          result.files_failed, result.files_not_attempted))
            finally:
                for file in files[:2]:
                    api.files.update(file.id, compilation_command=file.compilation_command)
                # Repair facts/index state even when a test assertion above fails.
                recovered = api.extractions.create(selection=selection, force=True)
                result = recovered.wait(timeout=180)
                retained(api.extractions, recovered, "restored-extraction")
                assert result.files_failed == 0 and result.files_processed == 3

            original = submitted[0][2]
            retry = api.extractions.retry(original.id)
            result = retry.wait(timeout=180)
            retained(api.extractions, retry, "retry-after-repair")
            assert retry.metadata.retry_of == original.id and retry.id != original.id
            assert result.files_processed == 3 and result.files_failed == 0
            assert len(api.extractions.failed_files(original.id).collect()) == 2
            reindex_id = cli_added_facts(api, files[2].path, evidence, save)
            for f in files:
                assert api.files.get(f.id).compilation_command == f.compilation_command
                assert hashlib.sha256(Path(f.path).read_bytes()).hexdigest() == original_hashes[f.path]
            save("final-repositories", [asdict(r) for r in api.repositories.list()])
            records, unstructured = [], []
            for line in Path(config["logging"]["file"]).read_text().splitlines():
                try:
                    records.append(json.loads(line))
                except json.JSONDecodeError:
                    unstructured.append(line)
            save("unstructured-log-lines", unstructured)
            for family, _, job in submitted:
                logs = [r for r in records if r.get("fields", {}).get("job_id") == job.id
                        and r["timestamp"] >= job.metadata.created_at]
                assert sum(r["event"] == "job.file_failed" for r in logs) == 2
                assert any(r["event"] == "job.diagnostic" for r in logs)
                assert any(r["event"] == "job.completed" and r["fields"]["files_failed"] == 2 for r in logs)
                save(f"{family}-logs", logs)
            save("summary", {"passed": True, "source_hashes_unchanged": original_hashes,
                              "reindex_job": reindex_id, "partial_jobs": [j.id for _, _, j in submitted]})
    print("Live continuation, recovery, logging, cancellation and REST reindex checks passed.", flush=True)


if __name__ == "__main__":
    main()
