import os
import signal
import subprocess
import time

import pytest

from conftest import records
from test_cli import invoke


@pytest.mark.parametrize("locking,expected", [("once", 0), ("always", 1)])
def test_dependency_lock_retry_is_bounded(batch, locking, expected):
    batch[0][2] = "dependency"
    batch[1]["BATCH_LOCK"] = locking
    result = invoke(batch, "-j", 3, *batch[2])
    assert result.returncode == expected, result.stdout + result.stderr
    rows = records(batch[3])
    assert len(rows) == len(batch[2])
    assert all(row["attempt"] == 2 for row in rows)
    if expected == 0:
        assert "5 succeeded" in result.stdout


def test_dependency_parse_failure_is_not_retried(batch, tmp_path):
    batch[0][2] = "dependency"
    failed = tmp_path / "fail.cpp"
    failed.touch()
    result = invoke(batch, failed)
    assert result.returncode != 0
    assert records(batch[3])[0]["attempt"] == 1


def test_cancel_during_lock_retry_reaps_child(batch):
    command, env, sources, trace = batch
    command[2] = "dependency"
    env.update(BATCH_LOCK="once", BATCH_RETRY_DELAY="30", BATCH_IGNORE_TERM="1")
    process = subprocess.Popen(
        command + ["-j", "3", *map(str, sources)],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    try:
        deadline = time.monotonic() + 5
        retried = []
        while time.monotonic() < deadline:
            retried = [row for row in records(trace) if row["attempt"] == 2]
            if retried:
                break
            time.sleep(0.025)
        assert len(retried) == 1
        process.send_signal(signal.SIGTERM)
        process.communicate(timeout=8)
        assert process.returncode != 0
        with pytest.raises(ProcessLookupError):
            os.kill(retried[0]["pid"], 0)
        assert sum(row["attempt"] for row in records(trace)) == 6
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
