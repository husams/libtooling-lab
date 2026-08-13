from __future__ import annotations

import importlib

_STEP_MODULES = (
    "steps.common_steps",
    "steps.database_steps",
    "steps.file_registry_steps",
    "steps.inheritance_steps",
    "steps.parameter_steps",
    "steps.record_steps",
    "steps.symbol_identity_steps",
    "steps.symbol_inventory_steps",
    "steps.symbol_stability_steps",
)


def load_step_definitions() -> None:
    for module in _STEP_MODULES:
        importlib.import_module(module)
