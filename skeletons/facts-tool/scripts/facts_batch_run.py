"""Process scheduling and output-directory locking."""

from pathlib import Path
import signal
import subprocess
import time

from facts_batch_lock import BatchRunError, lock_output, tool_path
from facts_batch_process import cleanup, command, reap, terminate
from facts_batch_retry import retry_locked


def run_batch(args, sources, output_names) -> int:
    output_dir = Path(args.output_dir).expanduser().resolve()
    tool = tool_path()
    lock = lock_output(output_dir)
    active = {}
    failed = []
    completed = 0
    next_source = 0
    interrupted = [0]

    def stop(signum, _frame):
        if not interrupted[0]:
            interrupted[0] = signum
            for process in active:
                terminate(process)

    old_handlers = {
        sig: signal.signal(sig, stop) for sig in (signal.SIGINT, signal.SIGTERM)
    }
    try:
        while (active and not interrupted[0]) or (
            next_source < len(sources) and not interrupted[0]
        ):
            while (
                len(active) < args.jobs
                and next_source < len(sources)
                and not interrupted[0]
            ):
                source = sources[next_source]
                next_source += 1
                database, log = output_names(source, output_dir, args.mode)
                try:
                    with log.open("w", encoding="utf-8") as stream:
                        process = subprocess.Popen(
                            command(tool, args, source, database),
                            stdout=stream,
                            stderr=subprocess.STDOUT,
                            start_new_session=True,
                        )
                except OSError as error:
                    for child in active:
                        terminate(child)
                    raise BatchRunError(
                        f"cannot start facts-tool for {source}: {error}"
                    ) from error
                active[process] = (source, log)
                if interrupted[0]:
                    terminate(process)
            for process, (source, log) in list(active.items()):
                status = process.poll()
                if status is None:
                    continue
                process.wait()
                del active[process]
                completed += 1
                if status:
                    failed.append((source, log, status))
            if active and not interrupted[0]:
                time.sleep(0.05)
        for process, (source, log) in list(active.items()):
            status = reap(process)
            del active[process]
            completed += 1
            failed.append((source, log, status or 128 + interrupted[0]))
        failed, _retry_count = retry_locked(
            args, failed, output_dir, tool, output_names, interrupted, active
        )
    finally:
        cleanup(active)
        for sig, handler in old_handlers.items():
            signal.signal(sig, handler)
        lock.close()
    for source, log, status in failed:
        print(f"FAIL {source} (exit {status}; see {log})")
    print(
        f"facts-tool-batch: {completed - len(failed)} succeeded, {len(failed)} failed"
    )
    return 128 + interrupted[0] if interrupted[0] else (1 if failed else 0)
