"""Compilation imports retain each translation unit's build-directory context."""
import json

import pytest

from contract import response_matches
from domain_http import index_ready
from support import eventually
from test_v2_analysis_jobs import rows, run


@pytest.mark.parametrize("ast_cache", [False, True])
def test_import_relative_commands_for_multiple_files_uses_active_clone(
        domain_project, server_factory, ast_cache):
    project = domain_project
    root = project.roots["alpha"]
    build = root / "build"
    build.mkdir()
    commands = []
    for name, expected in (("one", 1), ("two", 2)):
        directory = root / "src" / name
        (directory / "include").mkdir(parents=True)
        (directory / "include/value.hpp").write_text(
            f"#define VALUE {expected}\n")
        (directory / "include/forced.hpp").write_text(
            f"#define FORCED {expected}\n")
        (directory / "main.cpp").write_text(
            '#include "value.hpp"\n'
            "static_assert(VALUE == EXPECTED && FORCED == EXPECTED);\n"
            f"int imported_{name}() {{ return VALUE; }}\n")
        (directory / "flags.rsp").write_text(
            f"-std=c++17 -Iinclude -include forced.hpp -DEXPECTED={expected}\n")
        commands.append({"directory": f"../src/{name}", "file": "main.cpp",
                         "arguments": [project.compiler, "@flags.rsp", "-c", "main.cpp"]})
    (build / "compile_commands.json").write_text(json.dumps(commands))
    with project.defaults.open("a") as defaults:
        defaults.write(f"ast_cache: {str(ast_cache).lower()}\n")

    # The daemon runs outside the clone. Inactive clones and the daemon cwd
    # must never supply files, include paths, response files, or build options.
    inactive = project.inactive_clone().parent.parent
    (inactive / "build").mkdir()
    (inactive / "build/compile_commands.json").write_text("invalid json")
    server = server_factory(*project.options())
    index_ready(server.api)
    imported, location = run(server.api, "import", {
        "repository": "alpha", "compilation_database": "build/compile_commands.json"})
    assert imported["result"]["compilation_databases"] == 1
    assert rows(server.api, location, "databases")[0]["path"] == str(build / "compile_commands.json")
    # Narrowing discovery to build/ does not change the clone-relative base.
    reimported, _ = run(server.api, "import", {
        "selection": {"type": "directory", "repository": "alpha", "path": "build"},
        "compilation_database": "build/compile_commands.json"})
    assert reimported["result"]["files_registered"] == 0

    status, files = server.api.request("GET", "/api/v2/files?repository=alpha")
    assert status == 200, files
    for name in ("one", "two"):
        directory = root / "src" / name
        source = next(item for item in files["items"]
                      if item["path"] == str(directory / "main.cpp"))
        command = source["compilation_command"]
        assert command["working_directory"] == str(directory)
        assert "-I" + str(directory / "include") in command["arguments"]
        assert "@flags.rsp" not in command["arguments"]
        for header in ("value.hpp", "forced.hpp"):
            assert any(item["path"] == str(directory / "include" / header)
                       for item in files["items"])
    extracted, _ = run(server.api, "extract", {
        "selection": {"type": "files", "files": [
            {"path": f"src/{name}/main.cpp", "repository": "alpha"}
            for name in ("one", "two")]}})
    assert extracted["result"]["files_processed"] == 2


def test_failed_import_identifies_compilation_database_and_clone(domain_project, server_factory):
    project = domain_project
    root = project.roots["alpha"]
    source = project.sources["alpha"]
    source.write_text('#include "missing-import-header.hpp"\n')
    server = server_factory(*project.options())
    index_ready(server.api)
    status, submitted = server.api.request("POST", "/api/v2/import/job", {"repository": "alpha"})
    assert status == 202, submitted
    def finished():
        _, job = server.api.request("GET", f"/api/v2/import/job/{submitted['id']}")
        return job if job["state"] in {"succeeded", "failed"} else None

    job = eventually(finished, timeout=60)
    assert job["state"] == "failed", job
    _, document = server.api.request("GET", "/openapi.json")
    response_matches(document, "GET", "/api/v2/import/job/{id}", 200, job)
    details = job["error"]["details"]
    assert details["path"] == str(root / "compile_commands.json")
    assert details["repository"] == "alpha"
    assert details["clone_path"] == str(root)
    assert details["project_root"] == str(root)
    assert details["compilation_commands"][0]["working_directory"] == str(root)
    assert details["compilation_commands"][0]["source_file"] == str(source)
    assert str(source) in details["compilation_commands"][0]["arguments"]
    assert any("missing-import-header.hpp" in item["message"]
               for item in details["diagnostics"])
