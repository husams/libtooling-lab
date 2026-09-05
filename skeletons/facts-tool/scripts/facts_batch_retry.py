"""One deferred serial retry for SQLite contention."""

from pathlib import Path
import subprocess
import time

from facts_batch_process import command, reap, terminate


def retry_locked(
    args, failed, output_dir: Path, tool: str, output_names, interrupted, active
):
    if args.mode != "dependency" or interrupted[0]:
        return failed, 0
    deferred = []
    remaining = []
    for source, log, status in failed:
        try:
            text = log.read_text(encoding="utf-8")
        except OSError:
            text = ""
        if "database is locked" in text or "database table is locked" in text:
            deferred.append((source, log))
        else:
            remaining.append((source, log, status))
    completed = 0
    for source, log in deferred:
        if interrupted[0]:
            remaining.append((source, log, 128 + interrupted[0]))
            continue
        database, _ = output_names(source, output_dir, args.mode)
        with log.open("a", encoding="utf-8") as stream:
            stream.write("\n[facts-tool-batch] retrying after SQLite lock\n")
            process = subprocess.Popen(
                command(tool, args, source, database),
                stdout=stream,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
        active[process] = (source, log)
        if interrupted[0]:
            terminate(process)
        status = _wait_retry(process, interrupted)
        del active[process]
        completed += 1
        if status:
            remaining.append((source, log, status))
    return remaining, completed


def _wait_retry(process, interrupted):
    while True:
        status = process.poll()
        if status is not None:
            return status
        if interrupted[0]:
            return reap(process, force_after=1.0)
        time.sleep(0.05)
