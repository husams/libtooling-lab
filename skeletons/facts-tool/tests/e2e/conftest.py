from __future__ import annotations

from pathlib import Path

import pytest

from support.scenario import FactsToolContext

pytest_plugins = (
    "steps.recovery_steps",
    "steps.recovery_failure_steps",
    "steps.recovery_audit_steps",
    "steps.recovery_schema_steps",
    "steps.recovery_chain_steps",
    "steps.recovery_query_steps",
    "steps.recovery_header_steps",
    "steps.recovery_attempt_steps",
    "steps.recovery_partial_steps",
    "steps.recovery_discovery_steps",
    "steps.entry_callable_integration_steps",
    "steps.entry_graph_steps",
    "steps.entry_coverage_steps",
    "steps.entry_external_steps",
    "steps.entry_lifecycle_steps",
    "steps.entry_schema_steps",
    "steps.entry_provenance_steps",
    "steps.entry_rebuild_steps",
    "steps.entry_catalog_steps",
    "steps.entry_dependency_steps",
    "steps.entry_mutation_paths_steps",
    "steps.batch_steps",
    "steps.cli_yaml_precedence_steps",
    "steps.cli_yaml_precedence_assert_steps",
    "steps.cli_yaml_symbol_steps",
    "steps.cli_yaml_paths_steps",
    "steps.cli_yaml_paths_symbol_steps",
    "steps.cli_yaml_catalog_steps",
    "steps.extra_args_retention_steps",
    "steps.callable_steps",
    "steps.callable_persistence_steps",
    "steps.callable_semantics_steps",
    "steps.defaults_resolution_steps",
    "steps.defaults_validation_steps",
    "steps.defaults_policy_steps",
    "steps.defaults_compiler_steps",
    "steps.defaults_path_steps",
    "steps.defaults_facts_steps",
    "steps.defaults_anchor_steps",
    "steps.defaults_ownership_steps",
    "steps.alias_steps",
    "steps.b019_extraction_completeness_steps",
    "steps.call_graph_steps",
    "steps.s022_call_graph_steps",
    "steps.s022_review_steps",
    "steps.call_graph_query_steps",
    "steps.common_steps",
    "steps.configuration_defaults_steps",
    "steps.database_steps",
    "steps.dependent_template_type_steps",
    "steps.enumeration_steps",
    "steps.external_target_steps",
    "steps.forced_include_assert_steps",
    "steps.forced_include_import_steps",
    "steps.forced_include_path_steps",
    "steps.b027_external_callee_symbol_steps",
    "steps.b027_external_callee_failure_steps",
    "steps.implicit_allocation_target_steps",
    "steps.implicit_matrix_steps",
    "steps.implicit_stability_steps",
    "steps.implicit_storage_steps",
    "steps.implicit_traversal_steps",
    "steps.field_steps",
    "steps.variable_steps",
    "steps.file_registry_steps",
    "steps.header_defined_type_steps",
    "steps.inheritance_steps",
    "steps.initializer_dependent_alignment_steps",
    "steps.method_steps",
    "steps.parameter_default_steps",
    "steps.parameter_steps",
    "steps.return_type_steps",
    "steps.catalog_common_steps",
    "steps.catalog_repository_steps",
    "steps.catalog_component_steps",
    "steps.catalog_directory_steps",
    "steps.catalog_failure_steps",
    "steps.catalog_validation_steps",
    "steps.catalog_registration_steps",
    "steps.catalog_file_symbol_steps",
    "steps.project_import_steps",
    "steps.record_steps",
    "steps.references_steps",
    "steps.stored_compilation_steps",
    "steps.symbol_identity_steps",
    "steps.symbol_inventory_steps",
    "steps.symbol_stability_steps",
    "steps.template_steps",
    "steps.targeted_match_steps",
    "steps.native_matcher_workflow_steps",
    "steps.matched_symbol_index_steps",
    "steps.matched_symbol_identity_steps",
    "steps.matched_symbol_cli_steps",
    "steps.matched_symbol_invalid_steps",
    "steps.matched_symbol_lifecycle_steps",
    "steps.matched_symbol_performance_steps",
)


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("facts-tool e2e")
    group.addoption("--facts-tool", type=Path, required=True)
    group.addoption("--fixture-root", type=Path, required=True)
    group.addoption("--compiler", type=Path, required=True)
    group.addoption("--clang-driver", type=Path, default=None)
    group.addoption("--output-root", type=Path, required=True)


@pytest.fixture
def context(pytestconfig: pytest.Config) -> FactsToolContext:
    return FactsToolContext.create(
        facts_tool=pytestconfig.getoption("--facts-tool"),
        fixture_root=pytestconfig.getoption("--fixture-root"),
        compiler=pytestconfig.getoption("--compiler"),
        clang_driver=pytestconfig.getoption("--clang-driver"),
        output_root=pytestconfig.getoption("--output-root"),
    )
