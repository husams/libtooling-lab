"""Operations return domain data directly and asynchronously refresh search."""
from domain_http import await_symbols, completed, submit


def test_match_returns_named_bindings_and_indexes_new_symbol(domain_server, domain_project):
    source = domain_project.sources["alpha"]
    domain_project.write_source(source, "alpha", "matched")
    job = submit(domain_server.api, "/v1/matches", {"path": str(source)},
                 query='functionDecl(hasName("alpha::matched")).bind("selected")')
    result = completed(domain_server.api, job)["result"]
    assert result["operation"] == "match" and result["match_count"] == 1, result
    assert result["file"]["path"] == str(source) and result["file"]["repo"] == "alpha"
    binding = result["matches"][0]["bindings"]["selected"]
    assert binding["name"] == "alpha::matched" and binding["usr"], binding
    assert binding["location"]["path"] == str(source), binding
    item, = await_symbols(domain_server.api, "alpha::matched")["items"]
    assert item["usr"] == binding["usr"]


def test_dependencies_return_structured_file_edges(domain_server, domain_project):
    job = submit(domain_server.api, "/v1/dependencies",
                 {"path": "src/main.cpp", "repo": "beta"})
    result = completed(domain_server.api, job)["result"]
    assert result["operation"] == "dependencies" and result["edge_count"] >= 1, result
    assert result["file"]["path"] == str(domain_project.sources["beta"]), result
    assert len(result["edges"]) == result["edge_count"]
    names = dict(domain_project.rows("SELECT id,name FROM file"))
    assert any(names[edge["source_file_id"]] == "main.cpp" and
               names[edge["destination_file_id"]] == "common.hpp"
               for edge in result["edges"]), result


def test_invalid_clang_query_is_a_typed_job_error(domain_server):
    job = submit(domain_server.api, "/v1/matches",
                 {"path": "src/main.cpp", "repo": "alpha"}, query="notAClangMatcher()")
    result = domain_server.api.wait(job["id"])
    assert result["state"] == "failed" and result["result"] is None, result
    assert result["error"]["code"] and result["error"]["message"], result


def test_extraction_creates_server_managed_facts_for_unextracted_file(
        executable, compiler, tmp_path, server_factory):
    from domain_project import DomainProject
    from server import isolated_environment
    root = tmp_path / "unextracted"
    project = DomainProject(executable, root, compiler, isolated_environment(root))
    source = project.add("fresh", extract=False)
    server = server_factory(*project.options())
    result = completed(server.api, submit(server.api, "/v1/extractions", {"path": str(source)}))
    assert result["result"]["symbol_count"] > 0
    assert result["result"]["facts_committed"] is True
    assert await_symbols(server.api, "fresh::answer")["items"]
    assert project.rows("SELECT facts_db FROM file WHERE name='main.cpp'")[0][0]
