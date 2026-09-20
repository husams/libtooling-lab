"""Observe watcher state and query published facts through the public API."""
from support import eventually


def watch_status(api):
    status, body = api.request("GET", "/v1/watch")
    assert status == 200
    return body


def wait_cycle(api, previous):
    def finished():
        state = watch_status(api)
        if state["cycles"] <= previous or state["active"] or state["pending"]:
            return None
        return state
    state = eventually(finished)
    assert not state["last_error"], state
    return state


def symbols(api):
    return api.run([], "symbol/list")["stdout"]
