from __future__ import annotations

from pathlib import Path

import pytest

from steps import load_step_definitions
from support.bdd import ScenarioCase, collect_scenarios, run_scenario
from support.scenario import FactsToolContext

load_step_definitions()

SCENARIOS = collect_scenarios(Path(__file__).with_name("features"))


@pytest.mark.parametrize("scenario", SCENARIOS, ids=lambda case: case.test_id)
def test_facts_tool_scenario(
    scenario: ScenarioCase, facts_tool_context: FactsToolContext
) -> None:
    run_scenario(scenario, facts_tool_context)
