"""The typed resource contract drives native and Python API bindings."""
import yaml


def test_domain_routes_generate_sdk_methods_and_native_dispatch(
        source, generate, generated_files, tmp_path):
    output = tmp_path / "out"
    generate("--source", source, "--output-root", output)
    files = generated_files(output)
    routes = files["src/apis/generated/Routes.h"].decode()
    for operation in ("findSymbols", "indexStatus", "extract", "match", "dependencies"):
        assert "Operation::" + operation in routes
    for filename in ("resources.py", "async_resources.py"):
        code = files[f"python/src/facts_tool/rest/generated/{filename}"].decode()
        assert "def find_symbols(" in code and "def extract(" in code
        assert "FileSelector" in code and "body=arguments" not in code
        if filename.startswith("async"):
            assert "await async_request" in code


def test_request_db_path_addition_is_rejected_before_generation(source, generate, tmp_path):
    selector = source.parent / "schemas/file-selector.yaml"
    document = yaml.safe_load(selector.read_text())
    document["properties"]["project_db"] = {"type": "string"}
    selector.write_text(yaml.safe_dump(document, sort_keys=False))
    result = generate("--source", source, "--output-root", tmp_path / "out", code=2)
    assert "FileSelector request properties" in result.stderr
    assert not (tmp_path / "out/src/apis/generated").exists()


def test_matcher_query_must_remain_required(source, generate, tmp_path):
    matcher = source.parent / "schemas/match-request.yaml"
    document = yaml.safe_load(matcher.read_text())
    document["required"] = ["file"]
    matcher.write_text(yaml.safe_dump(document, sort_keys=False))
    result = generate("--source", source, "--output-root", tmp_path / "out", code=2)
    assert "MatchRequest.required" in result.stderr
