"""Clone identity returned by global lookup is reusable in typed requests."""
from domain_http import completed, index_ready, submit, symbols
from domain_project import DomainProject
from server import isolated_environment


def test_unlabelled_clone_lookup_identity_roundtrips(
        executable, compiler, tmp_path, server_factory):
    root = tmp_path / "unlabelled"
    project = DomainProject(executable, root, compiler, isolated_environment(root))
    source = project.add("alpha", label=False)
    server = server_factory(*project.options())
    index_ready(server.api)
    item, = symbols(server.api, "alpha::answer")["items"]
    assert item["clone"].isdigit(), item
    selector = {"path": "src/main.cpp", "repo": item["repo"], "clone": item["clone"]}
    result = completed(server.api, submit(server.api, "/v1/extractions", selector))["result"]
    assert result["file"]["path"] == str(source)
