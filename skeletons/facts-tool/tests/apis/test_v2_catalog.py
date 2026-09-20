"""Resource updates are atomic, nested, and preserve source files."""
import json
import sqlite3


def collection(api, resource, query=""):
    status, result = api.request("GET", f"/api/v2/{resource}{query}")
    assert status == 200, result
    return result["items"]


def repository(api, name="alpha"):
    return next(item for item in collection(api, "repositories") if item["name"] == name)


def test_repository_patch_rolls_back_every_field_on_invalid_clone(domain_server, domain_project):
    api = domain_server.api
    original = repository(api)
    path = f"/api/v2/repositories/{original['id']}"
    status, error = api.request("PATCH", path, {
        "name": "must-not-persist",
        "remote_url": "https://example.invalid/changed.git",
        "clones": [*original["clones"], {
            "path": str(domain_project.root / "does-not-exist"), "label": "broken"}],
    })
    assert status == 422, error
    assert api.request("GET", path) == (200, original)


def test_active_clone_must_belong_to_repository_and_failure_is_atomic(domain_server):
    api = domain_server.api
    first, other = repository(api), repository(api, "beta")
    path = f"/api/v2/repositories/{first['id']}"
    status, error = api.request("PATCH", path, {
        "name": "must-not-persist", "active_clone_id": other["active_clone_id"]})
    assert status == 409, error
    assert api.request("GET", path) == (200, first)


def test_clone_collection_replacement_preserves_ids_and_switches_atomically(domain_server, domain_project):
    api = domain_server.api
    original = repository(api)
    extra = domain_project.root / "replacement-clone"
    extra.mkdir()
    path = f"/api/v2/repositories/{original['id']}"
    status, updated = api.request("PATCH", path, {
        "clones": [*original["clones"], {"path": str(extra), "label": "replacement"}]})
    assert status == 200, updated
    assert updated["clones"][0]["id"] == original["clones"][0]["id"]
    replacement = next(clone for clone in updated["clones"] if clone["label"] == "replacement")
    status, error = api.request("PATCH", path, {"clones": [replacement]})
    assert status == 409, error
    assert api.request("GET", path) == (200, updated)
    status, switched = api.request("PATCH", path, {
        "clones": [replacement], "active_clone_id": replacement["id"]})
    assert status == 200, switched
    assert switched["active_clone_id"] == replacement["id"]
    assert switched["clones"] == [replacement]
    assert domain_project.sources["alpha"].is_file()


def test_file_patch_updates_compilation_and_invalidates_facts(domain_server, domain_project):
    api = domain_server.api
    files = collection(api, "files", "?repository=alpha")
    original = next(item for item in files if item["path"] == str(domain_project.sources["alpha"]))
    command = dict(original["compilation_command"])
    command["arguments"] = [*command["arguments"], "-DREST_PATCH_TEST=1"]
    registry_before = domain_project.rows("SELECT complete,fingerprint FROM project_registry WHERE id=1")
    path = f"/api/v2/files/{original['id']}"
    status, updated = api.request("PATCH", path, {"compilation_command": command})
    assert status == 200, updated
    assert updated["compilation_command"] == command
    assert updated["indexed"] is False
    stored = domain_project.rows(
        "SELECT compile_options,args_overridden,indexed FROM file WHERE id=?", (original["id"],))[0]
    assert json.loads(stored[0]) == command["arguments"]
    assert stored[1] == 1
    assert domain_project.rows("SELECT complete,fingerprint FROM project_registry WHERE id=1") == registry_before
    assert domain_project.rows(
        "SELECT file_id FROM api_index_invalidated_file WHERE file_id=?", (original["id"],))
    status, error = api.request("PATCH", path, {"compilation_command": {
        **command, "arguments": ["-DVALID=1", 123]}})
    assert status == 422, error
    assert api.request("GET", path)[1]["compilation_command"] == command


def test_deletion_requires_explicit_cascade_and_never_deletes_sources(domain_server, domain_project):
    api = domain_server.api
    repo = repository(api)
    path = f"/api/v2/repositories/{repo['id']}"
    status, error = api.request("DELETE", path)
    assert status == 409, error
    assert api.request("GET", path)[0] == 200
    assert api.request("DELETE", path + "?cascade=true") == (204, None)
    assert api.request("GET", path)[0] == 404
    assert domain_project.sources["alpha"].is_file()
    assert collection(api, "files", "?repository=alpha") == []
    assert collection(api, "files", "?repository=beta")


def test_catalog_pagination_and_unknown_fields(domain_server):
    api = domain_server.api
    status, first = api.request("GET", "/api/v2/repositories?limit=1")
    assert status == 200 and len(first["items"]) == 1 and first["next_cursor"]
    status, second = api.request("GET", "/api/v2/repositories?limit=1&cursor=" + first["next_cursor"])
    assert status == 200 and len(second["items"]) == 1 and second["next_cursor"] is None
    assert first["items"][0]["id"] != second["items"][0]["id"]
    status, error = api.request("PATCH", "/api/v2/repositories/" + first["items"][0]["id"], {"project_db": "forbidden"})
    assert status == 422, error
    assert api.request("GET", "/api/v2/repositories?limit=0")[0] == 422


def test_compilation_command_roundtrip_expands_aliases_and_preserves_flags(domain_server, domain_project):
    api = domain_server.api
    original = next(item for item in collection(api, "files", "?repository=alpha")
                    if item["path"] == str(domain_project.sources["alpha"]))
    with sqlite3.connect(domain_project.database) as database:
        database.execute("UPDATE file SET working_directory=?,compile_options=? WHERE id=?", (
            "<alpha-core>", json.dumps(["-std=c++17", "-I<alpha-core>", "-Werror"]), original["id"]))
    path = f"/api/v2/files/{original['id']}"
    status, file = api.request("GET", path)
    assert status == 200, file
    command = file["compilation_command"]
    assert command["working_directory"] == str(domain_project.sources["alpha"].parent)
    assert command["arguments"] == ["-std=c++17", "-I" + command["working_directory"], "-Werror"]
    command["arguments"].append("-DROUNDTRIP=1")
    status, updated = api.request("PATCH", path, {"compilation_command": command})
    assert status == 200, updated
    assert updated["compilation_command"] == command
