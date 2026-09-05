"""Executable lookup and output-directory locking."""

from pathlib import Path
import fcntl
import shutil


class BatchRunError(RuntimeError):
    """The batch cannot be started."""


def tool_path() -> str:
    tool = shutil.which("facts-tool")
    if not tool:
        raise BatchRunError("facts-tool executable was not found on PATH")
    return tool


def lock_output(path: Path):
    try:
        path.mkdir(parents=True, exist_ok=True)
        if not path.is_dir():
            raise BatchRunError(f"output directory is not a directory: {path}")
        handle = path.joinpath(".facts-tool-batch.lock").open("w")
        fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        return handle
    except BlockingIOError as error:
        raise BatchRunError(f"output directory is already locked: {path}") from error
    except OSError as error:
        raise BatchRunError(f"cannot lock output directory {path}: {error}") from error
