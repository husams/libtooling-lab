import os
import signal
import subprocess
import time

import pytest

from conftest import records
from test_cli import invoke


@pytest.mark.parametrize("jobs", [1, 2, 3])
def test_measured_concurrency_matches_limit(batch, jobs):
    batch[1]["BATCH_DELAY"] = ".4"
    result = invoke(batch, "-j", jobs, *batch[2])
    assert result.returncode == 0, result.stderr
    intervals = records(batch[3])
    assert len(intervals) == 5
    peak = max(
        sum(row["start"] <= event["start"] < row["end"] for row in intervals)
        for event in intervals
    )
    assert peak == jobs


@pytest.mark.parametrize("stop", [signal.SIGINT, signal.SIGTERM])
@pytest.mark.parametrize("ignore_term", [False, True])
def test_cancel_reaps_children_and_does_not_launch_queue(batch, stop, ignore_term):
    command, env, sources, trace = batch
    env["BATCH_DELAY"] = "30"
    if ignore_term:
        env["BATCH_IGNORE_TERM"] = "1"
    process = subprocess.Popen(
        command + ["-j", "2", *map(str, sources)],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    try:
        deadline = time.monotonic() + 5
        while len(list(trace.iterdir())) < 2 and time.monotonic() < deadline:
            time.sleep(0.025)
        assert len(list(trace.iterdir())) == 2
        process.send_signal(stop)
        process.communicate(timeout=8)
        assert process.returncode != 0
        assert len(list(trace.iterdir())) == 2
        for row in records(trace):
            with pytest.raises(ProcessLookupError):
                os.kill(row["pid"], 0)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()


def test_same_output_directory_is_locked(batch):
    command, env, sources, trace = batch
    env["BATCH_DELAY"] = "30"
    process = subprocess.Popen(
        command + ["-j", "1", str(sources[0])],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    try:
        deadline = time.monotonic() + 5
        while not list(trace.iterdir()) and time.monotonic() < deadline:
            time.sleep(0.025)
        assert list(trace.iterdir())
        result = invoke(batch, "-j", 1, sources[1])
        assert result.returncode != 0
        assert len(list(trace.iterdir())) == 1
    finally:
        process.terminate()
        process.communicate(timeout=8)
