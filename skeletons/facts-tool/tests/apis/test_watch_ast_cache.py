"""Watch edits must invalidate AST caches even at an unchanged Git commit."""
import subprocess
import sys

import pytest
from project import create_project
from support import eventually
from watch_support import symbols, wait_cycle, watch_status

pytestmark = pytest.mark.skipif(sys.platform != "linux", reason="inotify needs Linux")


def assert_fresh_match(api, source, name):
    matcher = f'functionDecl(hasName("{name}")).bind("symbol")'
    matched = api.run(["--matcher", matcher, str(source), "-v", "2"], "match")
    assert name in matched["stdout"] + matched["stderr"], matched


def test_uncommitted_source_and_header_edits_refresh_cached_ast(
        server_factory, compiler, tmp_path):
    root = tmp_path / "project"
    source, header = create_project(root, compiler)
    subprocess.run(["git", "init", "-q", str(root)], check=True)
    subprocess.run(["git", "-C", str(root), "add", "."], check=True)
    subprocess.run(["git", "-C", str(root), "-c", "user.name=API Test",
                    "-c", "user.email=api-test@example.invalid",
                    "commit", "-qm", "initial fixture"], check=True)
    config = tmp_path / "defaults.yaml"
    config.write_text(f"facts_template: {root / 'facts.db'}\nast_cache: true\n")
    server = server_factory("--watch", "--conf", root / "project.db",
                            "--config", config, "--debounce-ms", "50")
    imported = server.api.run(["-p", str(root), "-v", "2"], "import")
    assert "ast-cache: stored" in imported["stderr"], imported
    extracted = server.api.run(["-v", "2"], "extract")
    assert "ast-cache: hit" in extracted["stderr"], extracted
    eventually(lambda: str(root) in watch_status(server.api)["directories"])
    previous = watch_status(server.api)["cycles"]
    source.write_text(source.read_text() + "int fresh_source() { return 71; }\n")
    wait_cycle(server.api, previous)
    assert "fresh_source" in symbols(server.api)
    assert_fresh_match(server.api, source, "fresh_source")
    previous = watch_status(server.api)["cycles"]
    header.write_text(header.read_text() + "inline int fresh_header() { return 81; }\n")
    wait_cycle(server.api, previous)
    assert "fresh_header" in symbols(server.api)
    assert_fresh_match(server.api, source, "fresh_header")
