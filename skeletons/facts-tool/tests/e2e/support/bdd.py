from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional

StepDefinition = Callable[[Any], None]

_STEP_DEFINITIONS: dict[str, StepDefinition] = {}


@dataclass(frozen=True)
class Step:
    text: str
    line: int


@dataclass(frozen=True)
class Scenario:
    name: str
    steps: tuple[Step, ...]


@dataclass(frozen=True)
class Feature:
    name: str
    background: tuple[Step, ...]
    scenarios: tuple[Scenario, ...]


def step(text: str) -> Callable[[StepDefinition], StepDefinition]:
    def register(definition: StepDefinition) -> StepDefinition:
        if text in _STEP_DEFINITIONS:
            raise ValueError(f"duplicate step definition: {text}")
        _STEP_DEFINITIONS[text] = definition
        return definition

    return register


def parse_feature(path: Path) -> Feature:
    feature_name: Optional[str] = None
    background: list[Step] = []
    scenarios: list[Scenario] = []
    scenario_name: Optional[str] = None
    scenario_steps: list[Step] = []
    destination: Optional[list[Step]] = None

    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("Feature:"):
            feature_name = line.removeprefix("Feature:").strip()
            continue
        if line == "Background:":
            destination = background
            continue
        if line.startswith("Scenario:"):
            if scenario_name is not None:
                scenarios.append(Scenario(scenario_name, tuple(scenario_steps)))
            scenario_name = line.removeprefix("Scenario:").strip()
            scenario_steps = []
            destination = scenario_steps
            continue

        keyword, separator, text = line.partition(" ")
        if keyword in {"Given", "When", "Then", "And", "But"} and separator:
            if destination is None:
                raise ValueError(f"{path}:{line_number}: step is outside a scenario")
            destination.append(Step(text, line_number))
            continue
        if line.endswith(":"):
            raise ValueError(f"{path}:{line_number}: unsupported section: {line}")
        if destination is not None:
            raise ValueError(
                f"{path}:{line_number}: unrecognized scenario line: {line}"
            )

    if scenario_name is not None:
        scenarios.append(Scenario(scenario_name, tuple(scenario_steps)))
    if not feature_name:
        raise ValueError(f"{path}: missing Feature declaration")
    if not scenarios:
        raise ValueError(f"{path}: no scenarios")
    return Feature(feature_name, tuple(background), tuple(scenarios))


def run_features(feature_root: Path, context: Any) -> None:
    paths = sorted(feature_root.glob("*.feature"))
    if not paths:
        raise ValueError(f"no feature files found in {feature_root}")
    for path in paths:
        _run_feature(path, parse_feature(path), context)


def _run_feature(path: Path, feature: Feature, context: Any) -> None:
    print(f"Feature: {feature.name}", flush=True)
    for scenario in feature.scenarios:
        print(f"  Scenario: {scenario.name}", flush=True)
        for current_step in (*feature.background, *scenario.steps):
            print(f"    {current_step.text}", flush=True)
            definition = _STEP_DEFINITIONS.get(current_step.text)
            if definition is None:
                raise ValueError(
                    f"{path}:{current_step.line}: undefined step: {current_step.text}"
                )
            try:
                definition(context)
            except Exception as error:
                location = (
                    f"feature step: {path}:{current_step.line} "
                    f"({scenario.name}: {current_step.text})"
                )
                if hasattr(error, "add_note"):
                    error.add_note(location)
                else:
                    print(location, flush=True)
                raise
