"""Automatic extraction owns output routing for multi-repository templates."""
import sys

import pytest
from project import create_project, write_commands
from support import eventually
from test_watch_symlinks import settled
from watch_support import watch_status

pytestmark = pytest.mark.skipif(sys.platform != "linux", reason="inotify needs Linux")


def test_auto_extract_routes_each_source_under_its_clone_root(
        server_factory, compiler, tmp_path):
    project = tmp_path / "shared-project.db"
    config = tmp_path / "defaults.yaml"
    config.write_text('facts_template: "{project_root}/facts/{relative_path}/{filename}.db"\n')
    options = ("--conf", project, "--config", config, "--debounce-ms", "50")
    setup = server_factory("--no-watch", *options)
    outputs = []
    for name in ("alpha", "beta"):
        root = tmp_path / name
        source, _ = create_project(root, compiler)
        nested = root / "nested"
        nested.mkdir()
        extra = nested / "extra.cpp"
        extra.write_text(f"int {name}_extra() {{ return 17; }}\n")
        write_commands(root, compiler, [source, extra])
        # Initial registration uses an explicit invalidation target; the
        # automatic cycle must then derive every per-source output itself.
        setup.api.run(["-p", str(root), "-f", str(root / "seed.db")], "import")
        outputs.extend([root / "facts" / "sample.db", root / "facts" / "nested" / "extra.db"])
    setup.close()
    server = server_factory("--watch", *options)
    state = eventually(lambda: settled(server.api))
    assert state["ready"] and not state["last_error"], state
    assert all(path.is_file() for path in outputs), outputs
    for name in ("alpha", "beta"):
        def indexed():
            status, result = server.api.request("GET", f"/api/v2/symbols?qualified_name={name}_extra")
            return result["items"] if status == 200 else None
        items = eventually(indexed)
        assert len(items) == 1, items
    assert watch_status(server.api)["failures"] == 0


def test_managed_default_storage_supports_subsequent_import_cycles(
        executable, server_factory, compiler, tmp_path):
    root = tmp_path / "managed"
    source, _ = create_project(root, compiler)
    server = server_factory("--watch", "--debounce-ms", "50")
    status, repository = server.api.request("POST", "/api/v2/repositories", {
        "name": "managed", "clones": [{"label": "main", "path": str(root)}]})
    assert status == 201, repository
    state = eventually(lambda: settled(server.api))
    assert state["ready"] and not state["last_error"], state
    previous = state["cycles"]
    source.write_text(source.read_text() + "int managed_second_cycle() { return 23; }\n")
    def finished():
        state = settled(server.api)
        return state if state and state["cycles"] > previous else None
    state = eventually(finished)
    assert state["ready"] and not state["last_error"] and state["failures"] == 0, state
    def indexed():
        status, result = server.api.request("GET", "/api/v2/symbols?qualified_name=managed_second_cycle")
        return result.get("items") if status == 200 else None
    assert eventually(indexed)

    from test_watch_restart import restart
    checkpoint = server.config.with_name(server.config.name + ".watch-state.json")
    eventually(checkpoint.is_file)
    server.close()
    resumed = restart(executable, server, ("--debounce-ms", "50"))
    try:
        state = eventually(lambda: settled(resumed.api))
        assert state["resumed"] is True and state["cycles"] == 0, state
    finally:
        resumed.close()
    outputs = list((server.root / "data").rglob("clone-*.db"))
    assert len(outputs) == 1, outputs
    outputs[0].unlink()
    repaired = restart(executable, resumed, ("--debounce-ms", "50"))
    try:
        state = eventually(lambda: settled(repaired.api))
        assert not state["resumed"] and state["cycles"] > 0 and not state["last_error"], state
        assert outputs[0].is_file()
    finally:
        repaired.close()
