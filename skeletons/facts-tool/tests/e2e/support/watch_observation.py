"""Positive and negative watch observations, without mocking filesystem events."""
import time

from support.rest_http import eventually
from support.rest_project import wait_cycle, watch_status


def settled(catalog):
    previous, since = None, time.monotonic()
    def idle():
        nonlocal previous, since
        state = watch_status(catalog.server)
        if state["active"] or state["pending"] or state["scanning"] or state != previous:
            previous, since = state, time.monotonic()
        return state if time.monotonic() - since > 0.25 else None
    return eventually(idle)


def edit(path, name):
    path.write_text(f"int {name}() {{ return 99; }}\n")


def refresh(catalog, path, name):
    before = settled(catalog)["cycles"]
    edit(path, name)
    wait_cycle(catalog.server, before)
    eventually(lambda: name in catalog.symbols())


def stays_ignored(catalog, paths):
    before = settled(catalog)
    for index, path in enumerate(paths):
        edit(path, f"excluded_changed_{index}")
    deadline = time.monotonic() + 0.4
    while time.monotonic() < deadline:
        state = watch_status(catalog.server)
        assert state["cycles"] == before["cycles"], state
        assert not state["active"], state
        time.sleep(0.025)
