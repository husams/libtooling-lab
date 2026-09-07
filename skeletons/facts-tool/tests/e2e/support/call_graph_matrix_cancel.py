"""Cancellation case builders for the outcome matrix (B-042)."""
import signal
import sqlite3
import subprocess
import time

from support.callgraph_run import command


def _signal_after(argv, delay, env):
    process = subprocess.Popen(argv, env=env, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True)
    time.sleep(delay)
    process.send_signal(signal.SIGINT)
    stdout, stderr = process.communicate(timeout=60)
    return subprocess.CompletedProcess(argv, process.returncode, stdout, stderr)

def cancel_before_traversal(context, verbosity):
    # An exclusive lock on the facts store parks the tool in SQLite's busy
    # wait while it loads the graph, so SIGINT deterministically arrives
    # before the pre-traversal checkpoint at any verbosity.
    argv = command(context, "--function", "root", verbosity=verbosity)
    lock = sqlite3.connect(context.facts_database_path, isolation_level=None)
    lock.execute("BEGIN EXCLUSIVE")
    try:
        process = subprocess.Popen(argv, env=context.recovery_env,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True)
        time.sleep(0.5)
        process.send_signal(signal.SIGINT)
    finally:
        lock.execute("ROLLBACK")
        lock.close()
    stdout, stderr = process.communicate(timeout=60)
    return subprocess.CompletedProcess(argv, process.returncode, stdout, stderr)

def cancel_during_recovery(context, verbosity):
    source = context.recovery_sources[1]
    source.write_text(source.read_text() + "\n".join(
        f"struct MatrixCancelType{i} {{ int field; }};" for i in range(15000)))
    argv = command(context, "--function", "root", "--recover-missing", verbosity=verbosity)
    return _signal_after(argv, 0.2, context.recovery_env)
