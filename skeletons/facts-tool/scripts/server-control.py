#!/usr/bin/env python3
"""Operate this checkout's daemon without relying on a stale PID or fixed port."""

import argparse
import fcntl
import os
from pathlib import Path
import subprocess
import sys
import time

import yaml
from facts_tool.rest import ApiError, Client

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = Path(os.environ.get("FACTS_SERVER_RUNTIME", ROOT / ".server-runtime"))
CONFIG = RUNTIME / "server.yaml"


def settings():
    return yaml.safe_load(CONFIG.read_text())


def endpoint():
    value = settings()
    host = value["host"]
    return f"http://{'[' + host + ']' if ':' in host else host}:{value['port']}"


def client():
    return Client(endpoint(), token=os.environ.get("FACTS_TOOL_API_TOKEN"), timeout=60)


def locked():
    path = Path(str(CONFIG) + ".pid")
    if not path.exists():
        return False
    with path.open("r+") as handle:
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return True
        return False


def initialize():
    RUNTIME.mkdir(parents=True, exist_ok=True)
    for name in ("logs", "evidence", "facts"):
        (RUNTIME / name).mkdir(exist_ok=True)
    defaults = RUNTIME / "defaults.yaml"
    if not defaults.exists():
        defaults.write_text(yaml.safe_dump({
            "facts_template": str(RUNTIME / "facts/{relative_path}/{filename}.db"),
            "ast_cache": False,
            "ast_cache_dir": str(RUNTIME / "ast-cache"),
        }))
    if not CONFIG.exists():
        CONFIG.write_text(yaml.safe_dump({
            "schema_version": 1, "host": "127.0.0.1", "port": 0,
            "working_directory": str(ROOT),
            "logging": {"file": str(RUNTIME / "logs/server.jsonl"), "level": "debug"},
            "watch": {
                "enabled": True, "exclude_repositories": [], "exclude_clones": [],
                "exclude_directories": [".server-runtime", "build-rhel9", ".deps",
                                        ".venv-rhel9", ".cidx-artifacts", ".codex",
                                        ".pytest_cache", "tests/fixtures"],
                "exclude_patterns": [],
            },
            "defaults": ["--conf", str(RUNTIME / "project.db"),
                         "--config", str(defaults)],
            "import_arguments": ["-p", str(ROOT / "build-rhel9")],
            "extract_arguments": [], "debounce_ms": 1000, "timeout_seconds": 3600,
        }, sort_keys=False))


def describe():
    with client() as api:
        health = api.server.health()
        index = api.index.status()
        print(f"Endpoint: {endpoint()} (health: {health.status}, index: {index.state})")
        for repo in api.repositories.list():
            print(f"Repository: {repo.name}, indexed sources: {repo.indexed_source_count}/{repo.source_count}")
        watch = api.watcher.status()
        print(f"Watcher: enabled={watch.enabled}, active={watch.active}, pending={watch.pending}")
        if watch.last_error:
            print(f"Watcher error: {watch.last_error}")
    print(f"Config: {CONFIG}\nLog: {settings()['logging']['file']}")


def wait_for_catalog(timeout):
    deadline = time.monotonic() + timeout
    with client() as api:
        while True:
            try:
                api.repositories.list(limit=1).collect()
                return
            except ApiError as error:
                if error.status_code != 503 or time.monotonic() >= deadline:
                    raise
                time.sleep(0.1)


def start(timeout):
    initialize()
    if locked():
        wait_for_catalog(timeout)
        describe()
        print("Instance already running.")
        return
    binary = Path(os.environ.get("FACTS_SERVER_BINARY", RUNTIME / "build/facts-tool"))
    subprocess.run([str(binary), "serve", "--server-config", str(CONFIG), "--daemon"],
                   cwd=ROOT, check=True)
    wait_for_catalog(timeout)
    describe()


def stop(timeout):
    if not CONFIG.exists() or not locked():
        print("Instance already stopped.")
        return
    with client() as api:
        api.server.shutdown()
    deadline = time.monotonic() + timeout
    while locked():
        if time.monotonic() >= deadline:
            raise TimeoutError(f"Shutdown still draining jobs; inspect {settings()['logging']['file']}")
        time.sleep(0.2)
    print("Instance stopped; PID lock released.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("start", "stop", "status"))
    parser.add_argument("--timeout", type=float, default=300)
    args = parser.parse_args()
    try:
        {"start": lambda: start(args.timeout), "stop": lambda: stop(args.timeout),
         "status": describe}[args.action]()
    except Exception as error:
        print(f"facts-tool server: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
