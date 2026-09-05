import json
import os
import sys
from pathlib import Path

import pytest

WRAPPER = Path(__file__).resolve().parents[2] / "scripts" / "facts-tool-batch"


@pytest.fixture
def batch(tmp_path):
    tool = tmp_path / "facts-tool"
    tool.write_text(
        f"#!{sys.executable}\n"
        "import hashlib, json, os, pathlib, signal, sys, time\n"
        "if os.environ.get('BATCH_IGNORE_TERM'): signal.signal(signal.SIGTERM, signal.SIG_IGN)\n"
        "args = sys.argv[1:]\n"
        "source = pathlib.Path(args[-1])\n"
        "out = pathlib.Path(args[args.index('--output') + 1])\n"
        "record = {'args': args, 'pid': os.getpid(), 'start': time.time()}\n"
        "trace = pathlib.Path(os.environ['BATCH_TRACE']) / hashlib.sha256(str(source).encode()).hexdigest()\n"
        "record['attempt'] = json.loads(trace.read_text())['attempt'] + 1 if trace.exists() else 1\n"
        "trace.write_text(json.dumps(record))\n"
        "print('child diagnostic', flush=True)\n"
        "delay_key = 'BATCH_RETRY_DELAY' if record['attempt'] > 1 else 'BATCH_DELAY'\n"
        "time.sleep(float(os.environ.get(delay_key, '.15')))\n"
        "record['end'] = time.time()\n"
        "trace.write_text(json.dumps(record))\n"
        "if os.environ.get('BATCH_LOCK') == 'always' or (os.environ.get('BATCH_LOCK') == 'once' and record['attempt'] == 1):\n"
        "    print('facts-tool: database is locked', flush=True)\n"
        "    sys.exit(1)\n"
        "out.write_text(source.name)\n"
        "sys.exit(7 if source.name.startswith('fail') else 0)\n"
    )
    tool.chmod(0o755)
    trace = tmp_path / "trace"
    trace.mkdir()
    sources = [tmp_path / f"unit {i}.cpp" for i in range(5)]
    for source in sources:
        source.touch()
    env = dict(os.environ, PATH=str(tmp_path), BATCH_TRACE=str(trace))
    command = [sys.executable, str(WRAPPER), "extract", "-o", str(tmp_path / "out")]
    return command, env, sources, trace


def records(trace):
    result = []
    for path in trace.iterdir():
        try:
            result.append(json.loads(path.read_text()))
        except json.JSONDecodeError:
            # A running mock may be between truncate and write during polling.
            continue
    return result
