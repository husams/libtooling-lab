from __future__ import annotations

from pathlib import Path

import pytest

from support.scenario import FactsToolContext

pytest_plugins = (
    "steps.alias_steps",
    "steps.common_steps",
    "steps.database_steps",
    "steps.enumeration_steps",
    "steps.field_steps",
    "steps.variable_steps",
    "steps.file_registry_steps",
    "steps.header_defined_type_steps",
    "steps.inheritance_steps",
    "steps.method_steps",
    "steps.parameter_default_steps",
    "steps.parameter_steps",
    "steps.record_steps",
    "steps.symbol_identity_steps",
    "steps.symbol_inventory_steps",
    "steps.symbol_stability_steps",
    "steps.template_steps",
)


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("facts-tool e2e")
    group.addoption("--facts-tool", type=Path, required=True)
    group.addoption("--fixture-root", type=Path, required=True)
    group.addoption("--compiler", type=Path, required=True)
    group.addoption("--output-root", type=Path, required=True)


@pytest.fixture
def context(pytestconfig: pytest.Config) -> FactsToolContext:
    return FactsToolContext.create(
        facts_tool=pytestconfig.getoption("--facts-tool"),
        fixture_root=pytestconfig.getoption("--fixture-root"),
        compiler=pytestconfig.getoption("--compiler"),
        output_root=pytestconfig.getoption("--output-root"),
    )
