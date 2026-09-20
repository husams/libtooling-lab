"""A restart reuses only a successful content-checked watch baseline."""
import os
import re
import subprocess
import sys

import pytest
from server import Server
from support import eventually
from watch_support import symbols, watch_status
from test_watch_symlinks import prepare, settled

pytestmark = pytest.mark.skipif(sys.platform != "linux", reason="inotify needs Linux")


def restart(executable, previous, options):
    # Server's test harness otherwise reads the previous ephemeral port before
    # the child publishes its replacement. Keep the same config/checkpoint.
    previous.config.write_text(re.sub(r"(?m)^port:.*$", "port: 0", previous.config.read_text()))
    return Server(executable, previous.root, ("--watch", *options))


def test_unchanged_restart_skips_work_but_uncommitted_header_change_reprocesses(
        executable, server_factory, compiler, tmp_path):
    root, _, header, options = prepare(server_factory, compiler, tmp_path)
    subprocess.run(["git", "init", "-q", str(root)], check=True)
    subprocess.run(["git", "-C", str(root), "add", "sample.cpp", "sample.hpp", "compile_commands.json"], check=True)
    subprocess.run(["git", "-C", str(root), "-c", "user.name=API Test", "-c",
                    "user.email=api-test@example.invalid", "commit", "-qm", "baseline"], check=True)
    server = server_factory("--watch", *options)
    eventually(lambda: settled(server.api))
    checkpoint = server.config.with_name(server.config.name + ".watch-state.json")
    eventually(checkpoint.is_file)
    server.close()

    restarted = restart(executable, server, options)
    try:
        state = eventually(lambda: settled(restarted.api))
        assert state["resumed"] is True, state
        assert state["cycles"] == 0 and state["latest_jobs"] == [], state
        assert "answer" in symbols(restarted.api)
    finally:
        restarted.close()

    # Content, not only Git commit or mtime, defines the reusable baseline.
    timestamps = header.stat()
    header.write_text(header.read_text() + "inline int changed_while_stopped() { return 29; }\n")
    os.utime(header, ns=(timestamps.st_atime_ns, timestamps.st_mtime_ns))
    changed = restart(executable, server, options)
    try:
        state = eventually(lambda: settled(changed.api))
        assert state["resumed"] is False and state["cycles"] > 0, state
        assert "changed_while_stopped" in symbols(changed.api)
    finally:
        changed.close()

    # A source digest alone cannot establish that its indexed output survives.
    (root / "facts.db").unlink()
    missing_output = restart(executable, changed, options)
    try:
        state = eventually(lambda: settled(missing_output.api))
        assert state["resumed"] is False and state["cycles"] > 0, state
        assert "changed_while_stopped" in symbols(missing_output.api)
        assert (root / "facts.db").is_file()
    finally:
        missing_output.close()
