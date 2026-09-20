"""Checked-in executable bindings are reproducible and contract-driven."""
import ast

import yaml
from openapi_spec_validator import OpenAPIV31SpecValidator, validate


def test_split_yaml_is_valid_without_the_repository_bundler(source):
    OpenAPIV31SpecValidator(yaml.safe_load(source.read_text()), base_uri=source.as_uri()).validate()


def test_tracked_outputs_have_no_generation_drift(generate):
    generate("--check")


def test_isolated_generation_is_deterministic(source, generate, generated_files, tmp_path):
    output, exported = tmp_path / "output", tmp_path / "bundled.yaml"
    generate("--source", source, "--output-root", output, "--bundle", exported)
    validate(yaml.safe_load(exported.read_text()))
    first = generated_files(output)
    assert any(name.endswith(".h") for name in first)
    assert any(name.endswith(".py") for name in first)
    assert all(len(content.splitlines()) <= 100 for content in first.values())
    generate("--source", source, "--output-root", output)
    assert generated_files(output) == first
    generate("--source", source, "--output-root", output, "--check")


def test_stale_generated_file_is_rejected(source, generate, tmp_path):
    output = tmp_path / "output"
    generate("--source", source, "--output-root", output)
    stale = output / "src/apis/generated/Stale.h"
    stale.write_text("// stale generated artifact\n")
    result = generate("--source", source, "--output-root", output, "--check", code=1)
    assert "Stale.h" in result.stdout + result.stderr
    assert stale.exists(), "check mode must not modify generated files"


def test_changed_generated_file_is_rejected(source, generate, generated_files, tmp_path):
    output = tmp_path / "output"
    generate("--source", source, "--output-root", output)
    filename = next(name for name in generated_files(output) if name.endswith(".h"))
    target = output / filename
    target.write_text(target.read_text() + "// drift\n")
    generate("--source", source, "--output-root", output, "--check", code=1)
    assert target.read_text().endswith("// drift\n")


def test_contract_route_and_limit_change_executable_bindings(
        source, generate, generated_files, tmp_path):
    output = tmp_path / "output"
    generate("--source", source, "--output-root", output)
    before = generated_files(output)
    document = yaml.safe_load(source.read_text())
    document["paths"]["/ready"] = document["paths"].pop("/health")
    document["x-facts-limits"]["maxArguments"] = 2048
    source.write_text(yaml.safe_dump(document, sort_keys=False))
    for filename in ("arguments.yaml", "job-request.yaml"):
        path = source.parent / "schemas" / filename
        schema = yaml.safe_load(path.read_text())
        schema["properties"]["arguments"]["maxItems"] = 2048
        if "x-max-combined-arguments" in schema:
            schema["x-max-combined-arguments"] = 2048
        path.write_text(yaml.safe_dump(schema, sort_keys=False))
    generate("--source", source, "--output-root", output)
    after = generated_files(output)
    changed = {name: data for name, data in after.items() if data != before.get(name)}
    assert any(name.endswith(".h") and b"2048" in data for name, data in changed.items())
    assert any(name.endswith((".h", ".cpp")) and b'"/ready"' in data
               for name, data in changed.items())
    routes = after["python/src/facts_tool/rest/generated/routes.py"].decode()
    bindings = next(node.value for node in ast.parse(routes).body
                    if isinstance(node, ast.AnnAssign) and node.target.id == "ROUTES")
    assert ast.literal_eval(bindings)["health"] == ("GET", "ready")
    assert "MAX_ARGUMENTS = 2048" in routes
