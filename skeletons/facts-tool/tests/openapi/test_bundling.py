"""External fragments use their own file scope; recursive components stay finite."""
import importlib

import pytest
import yaml


@pytest.fixture
def bundle(project, monkeypatch):
    monkeypatch.syspath_prepend(str(project / "scripts"))
    return importlib.import_module("openapi_codegen.bundling").bundle


def test_external_fragment_is_resolved_in_its_own_file(bundle, tmp_path):
    source = tmp_path / "openapi.yaml"
    source.write_text(yaml.safe_dump({"paths": {"/health": {"$ref": "path.yaml#/Route"}},
                                     "Thing": {"type": "integer"}}))
    (tmp_path / "path.yaml").write_text(yaml.safe_dump({
        "Route": {"get": {"schema": {"$ref": "#/Thing"}}},
        "Thing": {"type": "string"}}))
    assert bundle(source)["paths"]["/health"]["get"]["schema"] == {"type": "string"}


def test_recursive_external_component_retains_internal_reference(bundle, tmp_path):
    source = tmp_path / "openapi.yaml"
    source.write_text(yaml.safe_dump({"components": {"schemas": {
        "Node": {"$ref": "node.yaml"}}}}))
    (tmp_path / "node.yaml").write_text(yaml.safe_dump({
        "type": "object", "properties": {"child": {"$ref": "node.yaml"}}}))
    node = bundle(source)["components"]["schemas"]["Node"]
    assert node["properties"]["child"]["$ref"] == "#/components/schemas/Node"
