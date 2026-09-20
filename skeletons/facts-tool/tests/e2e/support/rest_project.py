"""Small source projects and watcher observations through public endpoints."""
import json

from support.rest_http import eventually


def write_commands(root, compiler, sources):
    records = [{"directory": str(root), "file": str(source),
                "arguments": [str(compiler), "-std=c++17", "-c", str(source)]}
               for source in sources]
    (root / "compile_commands.json").write_text(json.dumps(records))


def create_project(root, compiler):
    root.mkdir()
    source, header = root / "sample.cpp", root / "sample.hpp"
    header.write_text("#pragma once\ninline int answer() { return 42; }\n")
    source.write_text('#include "sample.hpp"\nint main() { return answer(); }\n')
    write_commands(root, compiler, [source])
    return source, header


def watch_status(server):
    status, body = server.api.request("GET", "/v1/watch")
    assert status == 200, body
    return body


def wait_cycle(server, previous):
    def completed():
        state = watch_status(server)
        return state if state["cycles"] > previous and not (
            state["active"] or state["pending"]) else None
    state = eventually(completed)
    assert not state["last_error"], state
    return state
