"""Startup reconciliation and symlink warnings use the real import/extract flow."""
import sys

import pytest
from logging_support import records
from project import create_project
from support import eventually
from watch_support import symbols, wait_cycle, watch_status

pytestmark = pytest.mark.skipif(sys.platform != "linux", reason="inotify needs Linux")


def prepare(server_factory, compiler, tmp_path):
    root = tmp_path / "project"
    source, header = create_project(root, compiler)
    config = tmp_path / "defaults.yaml"
    config.write_text(f"facts_template: {root / 'facts.db'}\n")
    options = ("--conf", root / "project.db", "--config", config, "--debounce-ms", "50")
    server = server_factory("--no-watch", *options)
    server.api.run(["-p", str(root)], "import")
    server.close()
    return root, source, header, options


def settled(api):
    state = watch_status(api)
    return state if (state["cycles"] or state.get("resumed")) and not any(state[key] for key in
        ("active", "pending", "scanning")) else None


def test_restart_automatically_imports_and_extracts_with_broken_links(
        server_factory, compiler, tmp_path):
    root, source, _, options = prepare(server_factory, compiler, tmp_path)
    source.write_text(source.read_text() + "int discovered_on_startup() { return 13; }\n")
    (root / "broken.hpp").symlink_to("absent.hpp")
    (root / "broken-directory").symlink_to("absent-directory", target_is_directory=True)
    (root / "self-cycle").symlink_to("self-cycle")
    (root / "loop").symlink_to(root, target_is_directory=True)
    log = tmp_path / "warnings.jsonl"
    server = server_factory("--watch", "--log-file", log, *options)
    state = eventually(lambda: settled(server.api))
    assert state["ready"] and not state["last_error"], state
    assert state["failures"] == 0
    assert "discovered_on_startup" in symbols(server.api)
    warnings = {warning["path"]: warning for warning in state["warnings"]}
    assert warnings[str(root / "broken.hpp")]["code"] == "broken_symlink"
    assert warnings[str(root / "broken-directory")]["code"] == "broken_symlink"
    assert warnings[str(root / "self-cycle")]["code"] == "symlink_cycle"
    assert warnings[str(root / "loop")]["code"] == "symlink_cycle"
    eventually(lambda: len([entry for entry in records(log)
                            if entry["event"] == "watch.scan.warning"]) >= 4)
    # An unrelated change triggers another scan without duplicating old warnings.
    previous = state["cycles"]
    source.write_text(source.read_text() + "int after_warning() { return 14; }\n")
    state = wait_cycle(server.api, previous)
    assert state["ready"] and "after_warning" in symbols(server.api)
    logged = [entry for entry in records(log) if entry["event"] == "watch.scan.warning"
              and entry["fields"]["path"] == str(root / "broken.hpp")]
    assert len(logged) == 1


def test_valid_directory_alias_and_repaired_link_remain_monitored(
        server_factory, compiler, tmp_path):
    root, source, _, options = prepare(server_factory, compiler, tmp_path)
    nested = root / "nested"
    nested.mkdir()
    header = nested / "linked.hpp"
    header.symlink_to("header.data")
    header.write_text("inline int through_link() { return 1; }\n")
    (root / "alias").symlink_to("nested", target_is_directory=True)
    (root / "another-alias").symlink_to("nested", target_is_directory=True)
    (root / "repairable.hpp").symlink_to("missing.hpp")
    source.write_text('#include "alias/linked.hpp"\n' + source.read_text())
    server = server_factory("--watch", *options)
    state = eventually(lambda: settled(server.api))
    assert state["ready"] and "through_link" in symbols(server.api)
    assert state["watched_directories"] < 20  # physical aliases cannot grow recursion
    previous = state["cycles"]
    header.write_text(header.read_text() + "inline int edited_link_target() { return 2; }\n")
    wait_cycle(server.api, previous)
    assert "edited_link_target" in symbols(server.api)
    previous = watch_status(server.api)["cycles"]
    (root / "missing.hpp").write_text("inline int repaired_header() { return 3; }\n")
    source.write_text('#include "repairable.hpp"\n' + source.read_text())
    state = wait_cycle(server.api, previous)
    assert state["ready"]
    assert all(warning["path"] != str(root / "repairable.hpp") for warning in state["warnings"])
    assert "repaired_header" in symbols(server.api)


def test_file_compilation_override_survives_background_reimport_and_restart(
        server_factory, compiler, tmp_path):
    root, source, _, options = prepare(server_factory, compiler, tmp_path)
    source.write_text(source.read_text() +
        "\n#if MANUAL_MODE == 7\nint manual_configuration() { return 7; }\n#endif\n")
    server = server_factory("--watch", *options)
    state = eventually(lambda: settled(server.api))
    status, page = server.api.request("GET", "/api/v2/files")
    assert status == 200, page
    file = next(item for item in page["items"] if item["path"] == str(source))
    command = dict(file["compilation_command"])
    command["arguments"] = [*command["arguments"], "-DMANUAL_MODE=7"]
    path = f"/api/v2/files/{file['id']}"
    status, updated = server.api.request("PATCH", path, {"compilation_command": command})
    assert status == 200, updated
    # Explicit file settings must govern the dependency/AST preparation and
    # extraction performed by the automatically scheduled import cycle.
    wait_cycle(server.api, state["cycles"])
    assert "manual_configuration" in symbols(server.api)
    eventually(lambda: server.api.request("GET", "/api/v2/symbols?qualified_name=manual_configuration")[1].get("items"))
    assert server.api.request("GET", path)[1]["compilation_command"] == command
    previous = watch_status(server.api)["cycles"]
    source.write_text(source.read_text() + "int after_manual_configuration() { return 1; }\n")
    wait_cycle(server.api, previous)
    assert server.api.request("GET", path)[1]["compilation_command"] == command
    server.close()
    restarted = server_factory("--watch", *options)
    state = eventually(lambda: settled(restarted.api))
    assert state["ready"] and not state["last_error"], state
    assert restarted.api.request("GET", path)[1]["compilation_command"] == command
    assert "manual_configuration" in symbols(restarted.api)
    eventually(lambda: restarted.api.request("GET", "/api/v2/symbols?qualified_name=manual_configuration")[1].get("items"))


def test_conflicting_compilation_databases_are_reported_before_import(
        server_factory, compiler, tmp_path):
    import json
    root, source, _, options = prepare(server_factory, compiler, tmp_path)
    alternative = root / "alternative-build"
    alternative.mkdir()
    commands = json.loads((root / "compile_commands.json").read_text())
    commands[0]["arguments"].append("-DCONFLICTING_DATABASE=1")
    database = alternative / "compile_commands.json"
    database.write_text(json.dumps(commands))
    server = server_factory("--watch", *options)
    def conflict():
        state = watch_status(server.api)
        return state if "conflicting compilation databases" in state["last_error"] else None
    state = eventually(conflict)
    assert server.api.request("GET", "/health")[0] == 200
    status, page = server.api.request("GET", "/api/v2/files")
    assert status == 200, page
    file = next(item for item in page["items"] if item["path"] == str(source))
    assert "-DCONFLICTING_DATABASE=1" not in file["compilation_command"]["arguments"]
    database.unlink()
    state = wait_cycle(server.api, state["cycles"])
    assert state["ready"] and not state["last_error"], state
