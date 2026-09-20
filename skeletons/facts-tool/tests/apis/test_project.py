"""Real C++ import, extraction, matcher and graph workflows through HTTP."""
from project import create_project, initialise


def test_import_extract_match_and_graph(server, compiler, tmp_path):
    root = tmp_path / "project"
    source, _ = create_project(root, compiler)
    project, facts = initialise(server.api, root)
    listing = server.api.run(["-c", project, "-f", facts], "symbol/list")
    assert "answer" in listing["stdout"]
    assert "main" in listing["stdout"]
    matcher = 'functionDecl(hasName("main")).bind("symbol")'
    match = server.api.run(["-c", project, "-f", facts, "--matcher", matcher,
                            str(source)], "match")
    assert "main" in match["stdout"] + match["stderr"]
    graph = server.api.run(["-c", project, "-f", facts, "--function", "main"],
                           "analyse/call-graph")
    assert "complete" in graph["stdout"] + graph["stderr"]
    browser = server.api.run(["-c", project, "-f", facts], "symbol/browser")
    assert "answer" in browser["stdout"]
    browser = server.api.run(["symbol", "-c", project, "-f", facts, "browser"])
    assert "answer" in browser["stdout"]


def test_saved_defaults_are_applied_to_jobs(server_factory, compiler, tmp_path):
    root = tmp_path / "project"
    create_project(root, compiler)
    project = root / "project.db"
    defaults = root / "defaults.yaml"
    defaults.write_text(f"facts_template: {root / 'facts.db'}\n")
    server = server_factory("--conf", project, "--config", defaults)
    server.api.run(["-p", str(root)], "import")
    server.api.run([], "extract")
    assert "answer" in server.api.run([], "symbol/list")["stdout"]
