"""V2 resource semantics and operation-specific schemas stay explicit."""
import json
import sys

import pytest
from jsonschema import Draft202012Validator


@pytest.fixture
def contract(project):
    sys.path.insert(0, str(project / "scripts"))
    from openapi_codegen.bundling import bundle
    return bundle(project / "src/apis/openapi/openapi.yaml")


def validator(contract, name):
    return Draft202012Validator({
        "$ref": f"#/components/schemas/V2{name}",
        "components": contract["components"],
    })


def test_methods_follow_resource_semantics(contract):
    for path, methods in contract["paths"].items():
        if not path.startswith("/api/v2/"):
            continue
        assert "/commands" not in path
        for method, operation in methods.items():
            if method in {"get", "delete"}:
                assert "requestBody" not in operation
    for family in ("extract", "match", "import", "dependencies", "callgraphs",
                   "variable-flow", "scan", "index"):
        prefix = f"/api/v2/{family}/job"
        assert set(contract["paths"][prefix]) == {"get", "post"}
        assert set(contract["paths"][prefix + "/{id}"]) == {"get", "delete"}
        assert set(contract["paths"][prefix + "/{id}/results"]) == {"get"}


def test_nested_registration_and_compiler_settings(contract):
    assert not any("/clones" in path or "compilation-command" in path
                   or "active-clone" in path for path in contract["paths"])
    validator(contract, "UpdateRepositoryRequest").validate({"active_clone_id": "2"})
    validator(contract, "UpdateFileRequest").validate({"compilation_command": {
        "driver": "clang++", "working_directory": "/repo", "arguments": ["-std=c++23"]}})
    assert list(validator(contract, "UpdateFileRequest").iter_errors({"arguments": []}))


def test_prefix_default_and_exact_option(contract):
    operation = contract["paths"]["/api/v2/symbols"]["get"]
    mode = next(p["schema"] for p in operation["parameters"] if p["name"] == "match")
    assert mode["default"] == "prefix"
    assert mode["enum"] == ["prefix", "exact"]
    assert "literal" in operation["description"]


def test_analysis_selectors_are_closed_and_unambiguous(contract):
    valid = {"selection": {"type": "files", "files": [{"path": "src/a.cpp"}]}}
    validator(contract, "ExtractionRequest").validate(valid)
    invalid = {**valid, "arguments": ["--conf", "/secret/project.db"]}
    assert list(validator(contract, "ExtractionRequest").iter_errors(invalid))
    assert list(validator(contract, "SymbolSelector").iter_errors({
        "qualified_name": "app::run", "usr": "c:@N@app@F@run#"}))
    assert list(validator(contract, "ScanRequest").iter_errors(valid))


def test_job_results_are_operation_specific(contract):
    schemas = contract["components"]["schemas"]
    for name in ("Extraction", "Match", "Dependencies", "Import", "CallGraph",
                 "VariableFlow", "Scan", "Index"):
        result = schemas[f"V2{name}Job"]["properties"]["result"]["anyOf"][0]
        assert result == {"$ref": f"#/components/schemas/V2{name}Result"}
        assert schemas[f"V2{name}Result"]["additionalProperties"] is False
    assert schemas["V2MatchRecord"]["properties"]["bindings"]["additionalProperties"] == {
        "$ref": "#/components/schemas/V2MatchNode"}
    assert "stdout" not in json.dumps(schemas["V2ExtractionResult"])


def test_empty_graph_page_and_broken_link_warning_are_valid(contract):
    validator(contract, "CallGraphResultPage").validate({"items": [], "next_cursor": None})
    validator(contract, "IndexResultPage").validate({
        "items": [{"index_revision": "7"}], "next_cursor": None,
    })
    validator(contract, "ScanWarning").validate({
        "code": "broken_symlink", "severity": "warning", "path": "/repo/missing",
        "target": "generated/not-yet-created", "action": "skipped",
        "message": "Symbolic link target does not exist",
    })
