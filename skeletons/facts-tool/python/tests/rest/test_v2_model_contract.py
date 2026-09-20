"""Ensure public Python records stay aligned with the independently served schema."""

from dataclasses import MISSING, fields
from importlib import import_module
from pathlib import Path

import pytest
import yaml

SCHEMAS = Path(__file__).resolve().parents[3] / "src/apis/openapi/v2/schemas"
MODELS = [
    ("catalog_models.Repository", "repository"),
    ("catalog_models.File", "file"),
    ("catalog_models.Component", "component"),
    ("catalog_models.Directory", "directory"),
    ("catalog_models.CompilationCommand", "compilation-command"),
    ("symbol_models.Symbol", "symbol"),
    ("symbol_models.SymbolOccurrence", "symbol-occurrence"),
    ("symbol_models.SymbolRelation", "symbol-relation"),
    ("analysis_models.ExtractionResult", "extraction-result"),
    ("analysis_models.FileExtraction", "extraction-file"),
    ("analysis_models.DependencyResult", "dependencies-result"),
    ("analysis_models.DependencyEdge", "dependency-edge"),
    ("match_models.MatchResult", "match-result"),
    ("match_models.MatchRow", "match-record"),
    ("match_models.MatchBinding", "match-node"),
    ("graph_models.CallGraphResult", "call-graph-result"),
    ("graph_models.CallGraphNode", "call-graph-node"),
    ("graph_models.CallGraphEdge", "call-graph-edge"),
    ("flow_models.VariableFlowResult", "variable-flow-result"),
    ("flow_models.FlowNode", "flow-node"),
    ("flow_models.FlowEdge", "flow-edge"),
    ("flow_models.FlowBoundary", "flow-boundary"),
    ("discovery_models.ImportResult", "import-result"),
    ("discovery_models.ScanResult", "scan-result"),
    ("discovery_models.ScanWarning", "scan-warning"),
    ("watch_models.WatcherStatus", "watcher-status"),
    ("watch_models.WatcherSettings", "watcher-settings"),
    ("index.IndexStatus", "index-status"),
    ("index.IndexResult", "index-result"),
]


@pytest.mark.parametrize("reference,schema_name", MODELS)
def test_record_fields_and_required_values_match_openapi(reference, schema_name):
    module, name = reference.split(".")
    model = getattr(import_module("facts_tool.rest.v2." + module), name)
    schema = yaml.safe_load((SCHEMAS / (schema_name + ".yaml")).read_text())
    model_fields = fields(model)
    assert {f.name for f in model_fields} == set(schema["properties"])
    required = {
        f.name
        for f in model_fields
        if f.default is MISSING and f.default_factory is MISSING
    }
    assert required == set(schema.get("required", []))
