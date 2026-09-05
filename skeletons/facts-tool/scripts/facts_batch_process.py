"""facts-tool child process helpers."""

import os
import signal
import subprocess


def command(tool: str, args, source, database) -> list[str]:
    result = (
        [tool, "extract"] if args.mode == "extract" else [tool, "analyse", "dependency"]
    )
    result += ["--output", str(database)]
    if args.conf is not None:
        result += ["--conf", args.conf]
    if args.config is not None:
        result += ["--config", args.config]
    for extra in args.extra_args:
        result += ["--extra-arg", extra]
    return result + ["--verbose", str(args.verbose), "--", str(source)]


def terminate(process: subprocess.Popen) -> None:
    try:
        if hasattr(os, "killpg"):
            os.killpg(process.pid, signal.SIGTERM)
        else:
            process.terminate()
    except (OSError, ProcessLookupError):
        pass


def reap(process: subprocess.Popen, force_after: float = 1.0) -> int:
    try:
        return process.wait(timeout=force_after)
    except subprocess.TimeoutExpired:
        try:
            if hasattr(os, "killpg"):
                os.killpg(process.pid, signal.SIGKILL)
            else:
                process.kill()
        except (OSError, ProcessLookupError):
            pass
        return process.wait()


def cleanup(active: dict) -> None:
    for process in active:
        terminate(process)
    for process in list(active):
        reap(process, force_after=1.0)
        del active[process]
