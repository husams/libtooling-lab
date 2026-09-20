"""Linux inotify integration including atomic saves and new subdirectories."""
import sys

import pytest
from project import create_project, write_commands
from support import eventually
from watch_support import symbols, wait_cycle, watch_status

pytestmark = pytest.mark.skipif(sys.platform != "linux", reason="inotify needs Linux")


@pytest.fixture
def watched(server_factory, compiler, tmp_path):
    root = tmp_path / "project"
    source, header = create_project(root, compiler)
    config = tmp_path / "defaults.yaml"
    config.write_text(f"facts_template: {root / 'facts.db'}\n")
    server = server_factory("--watch", "--conf", root / "project.db",
                            "--config", config, "--debounce-ms", "50")
    server.api.run(["-p", str(root)], "import")
    server.api.run([], "extract")
    state = eventually(lambda: (status if str(root) in status["directories"] else None)
                       if (status := watch_status(server.api)) else None)
    assert state["backend"] == "inotify"
    assert state["enabled"]
    assert str(root) in state["directories"]
    return server, root, source, header


def test_source_and_header_edits_reindex(watched):
    server, _, source, header = watched
    previous = watch_status(server.api)["cycles"]
    source.write_text(source.read_text() + "int added_source() { return 7; }\n")
    wait_cycle(server.api, previous)
    assert "added_source" in symbols(server.api)
    previous = watch_status(server.api)["cycles"]
    header.write_text(header.read_text() + "inline int added_header() { return 8; }\n")
    wait_cycle(server.api, previous)
    assert "added_header" in symbols(server.api)


def test_atomic_save_reindexes(watched):
    server, root, source, _ = watched
    previous = watch_status(server.api)["cycles"]
    replacement = root / "replacement.tmp"
    replacement.write_text(source.read_text() + "int atomic_save() { return 9; }\n")
    replacement.replace(source)
    wait_cycle(server.api, previous)
    assert "atomic_save" in symbols(server.api)


def test_new_nested_directory_and_compile_commands(watched, compiler):
    server, root, source, _ = watched
    previous = watch_status(server.api)["cycles"]
    nested = root / "new" / "nested"
    nested.mkdir(parents=True)
    added = nested / "new.cpp"
    added.write_text("int newly_registered() { return 10; }\n")
    write_commands(root, compiler, [source, added])
    wait_cycle(server.api, previous)
    assert "newly_registered" in symbols(server.api)
    previous = watch_status(server.api)["cycles"]
    added.write_text("int changed_nested() { return 11; }\n")
    wait_cycle(server.api, previous)
    assert "changed_nested" in symbols(server.api)


def test_failed_reimport_is_visible(watched):
    server, root, _, _ = watched
    previous = watch_status(server.api)["failures"]
    (root / "compile_commands.json").write_text("invalid JSON")
    def failed():
        state = watch_status(server.api)
        return state if state["failures"] > previous else None
    state = eventually(failed)
    assert state["last_error"]
    assert server.api.request("GET", "/health")[0] == 200
