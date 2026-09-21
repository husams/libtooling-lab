"""Every selected clone supplies its own configuration, even outside the daemon cwd."""
import json
import sqlite3

from domain_http import index_ready
from test_v2_analysis_jobs import run


def test_clone_configuration_applies_to_import_extract_and_match(domain_project, server_factory):
    project = domain_project
    for name, number in (("alpha", 11), ("beta", 22)):
        root, source = project.roots[name], project.sources[name]
        (root / ".facts-tool.yaml").write_text(f"extra_args: [-DWORKSPACE_VALUE={number}]\n")
        source.write_text(f"static_assert(WORKSPACE_VALUE == {number});\nint {name}_workspace() {{ int tracked = WORKSPACE_VALUE; return tracked; }}\n")
    server = server_factory(*project.options())
    index_ready(server.api)
    for name in ("alpha", "beta"):
        run(server.api, "import", {"repository": name})
    selection = {"type": "files", "files": [
        {"repository": name, "path": "src/main.cpp"} for name in ("alpha", "beta")]}
    extracted, _ = run(server.api, "extract", {"selection": selection, "force": True})
    assert extracted["result"]["files_processed"] == 2
    matched, _ = run(server.api, "match", {"selection": selection, "expression": "functionDecl()"})
    assert matched["result"]["match_count"] >= 2
    flow, _ = run(server.api, "variable-flow", {
        "function": {"qualified_name": "alpha_workspace"}, "variable": {"name": "tracked"},
        "selection": {"type": "all"}, "interprocedural": False})
    assert flow["result"]["node_count"] > 0


def test_relative_working_directory_and_facts_use_active_clone(domain_project, server_factory):
    project = domain_project
    root, source = project.roots["alpha"], project.sources["alpha"]
    (source.parent / "include").mkdir()
    (source.parent / "include/workspace.hpp").write_text("#define WORKSPACE_VALUE 42\n")
    source.write_text('#include "workspace.hpp"\nint relative_workspace() { return WORKSPACE_VALUE; }\n')
    # Stored relative build directories are component-relative (component is src/).
    with sqlite3.connect(project.database) as db:
        db.execute("UPDATE file SET working_directory='.', compile_options=?, facts_db='manual.db' "
                   "WHERE name='main.cpp' AND directory_id IN "
                   "(SELECT d.id FROM directory d JOIN component c ON c.id=d.component_id "
                   "JOIN repository r ON r.id=c.repository_id WHERE r.name='alpha')",
                   (json.dumps(["-std=c++17", "-Iinclude"]),))
    server = server_factory(*project.options())
    index_ready(server.api)
    selection = {"type": "files", "files": [{"repository": "alpha", "path": "src/main.cpp"}]}
    run(server.api, "extract", {"selection": selection, "force": True})
    assert (root / "manual.db").exists()
    assert not (server.root / "manual.db").exists()
    run(server.api, "match", {"selection": selection, "expression": "functionDecl()"})
