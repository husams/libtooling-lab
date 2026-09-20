"""Malformed contracts fail without replacing usable generated code."""
import pytest
import yaml


@pytest.mark.parametrize("reference", ["https://example.invalid/schema.yaml", "../outside.yaml"])
def test_external_reference_boundaries(source, generate, tmp_path, reference):
    document = yaml.safe_load(source.read_text())
    document["paths"]["/health"] = {"$ref": reference}
    source.write_text(yaml.safe_dump(document, sort_keys=False))
    result = generate("--source", source, "--output-root", tmp_path / "out", code=2)
    assert result.stderr
    assert not (tmp_path / "out/src/apis/generated").exists()


def test_duplicate_yaml_keys_are_rejected(source, generate, tmp_path):
    source.write_text(source.read_text() + '\nopenapi: "3.1.0"\n')
    result = generate("--source", source, "--output-root", tmp_path / "out", code=2)
    assert "duplicate" in result.stderr.lower()


def test_invalid_contract_does_not_overwrite_bindings(
        source, generate, generated_files, tmp_path):
    output = tmp_path / "out"
    generate("--source", source, "--output-root", output)
    before = generated_files(output)
    document = yaml.safe_load(source.read_text())
    document["paths"]["/duplicate-health"] = document["paths"]["/health"]
    source.write_text(yaml.safe_dump(document, sort_keys=False))
    generate("--source", source, "--output-root", output, code=2)
    assert generated_files(output) == before


@pytest.mark.parametrize("key,value", [("maxArguments", 2048),
                                      ("maxHeaderBytes", 2 ** 32)])
def test_native_limit_mismatch_and_overflow_are_rejected(source, generate, tmp_path, key, value):
    document = yaml.safe_load(source.read_text())
    document["x-facts-limits"][key] = value
    source.write_text(yaml.safe_dump(document, sort_keys=False))
    generate("--source", source, "--output-root", tmp_path / "out", code=2)


def test_unsupported_parameter_rename_cannot_replace_working_bindings(
        source, generate, generated_files, tmp_path):
    output = tmp_path / "out"
    generate("--source", source, "--output-root", output)
    before = generated_files(output)
    document = yaml.safe_load(source.read_text())
    document["paths"]["/v1/commands/{path}"] = document["paths"].pop(
        "/v1/commands/{commandPath}")
    source.write_text(yaml.safe_dump(document, sort_keys=False))
    path_file = source.parent / "paths/command.yaml"
    operation = yaml.safe_load(path_file.read_text())
    operation["parameters"][0]["name"] = "path"
    path_file.write_text(yaml.safe_dump(operation, sort_keys=False))
    generate("--source", source, "--output-root", output, code=2)
    assert generated_files(output) == before
