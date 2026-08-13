from __future__ import annotations

import re
from collections.abc import Callable
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Any, Optional

StepDefinition = Callable[..., None]
Table = tuple[tuple[str, ...], ...]

_STEP_DEFINITIONS: dict[tuple[str, str], StepDefinition] = {}


@dataclass(frozen=True)
class Step:
    keyword: str
    definition_keyword: str
    text: str
    line: int
    table: Table = ()


@dataclass(frozen=True)
class Scenario:
    name: str
    steps: tuple[Step, ...]


@dataclass(frozen=True)
class ScenarioTemplate:
    name: str
    steps: tuple[Step, ...]
    outline: bool
    examples: Table


@dataclass(frozen=True)
class Feature:
    name: str
    background: tuple[Step, ...]
    scenarios: tuple[Scenario, ...]


@dataclass(frozen=True)
class ScenarioCase:
    path: Path
    feature: Feature
    scenario: Scenario

    @property
    def test_id(self) -> str:
        return f"{self.path.stem}: {self.scenario.name}"


def _register_step(
    keyword: str, text: str
) -> Callable[[StepDefinition], StepDefinition]:
    def register(definition: StepDefinition) -> StepDefinition:
        key = (keyword, text)
        if key in _STEP_DEFINITIONS:
            raise ValueError(f"duplicate {keyword} step definition: {text}")
        _STEP_DEFINITIONS[key] = definition
        return definition

    return register


def given(text: str) -> Callable[[StepDefinition], StepDefinition]:
    return _register_step("Given", text)


def when(text: str) -> Callable[[StepDefinition], StepDefinition]:
    return _register_step("When", text)


def then(text: str) -> Callable[[StepDefinition], StepDefinition]:
    return _register_step("Then", text)


def parse_feature(path: Path) -> Feature:
    feature_name: Optional[str] = None
    background: list[Step] = []
    templates: list[ScenarioTemplate] = []
    scenario_name: Optional[str] = None
    scenario_steps: list[Step] = []
    scenario_outline = False
    examples: list[tuple[str, ...]] = []
    reading_examples = False
    destination: Optional[list[Step]] = None
    definition_keyword: Optional[str] = None

    def append_scenario() -> None:
        if scenario_name is None:
            return
        templates.append(
            ScenarioTemplate(
                scenario_name,
                tuple(scenario_steps),
                scenario_outline,
                tuple(examples),
            )
        )

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
            definition_keyword = None
            reading_examples = False
            continue
        if line.startswith("Scenario Outline:") or line.startswith("Scenario:"):
            append_scenario()
            scenario_outline = line.startswith("Scenario Outline:")
            prefix = "Scenario Outline:" if scenario_outline else "Scenario:"
            scenario_name = line.removeprefix(prefix).strip()
            scenario_steps = []
            examples = []
            reading_examples = False
            destination = scenario_steps
            definition_keyword = None
            continue
        if line == "Examples:":
            if scenario_name is None or not scenario_outline:
                raise ValueError(
                    f"{path}:{line_number}: Examples requires a Scenario Outline"
                )
            reading_examples = True
            destination = None
            continue

        keyword, separator, text = line.partition(" ")
        if keyword in {"Given", "When", "Then", "And", "But"} and separator:
            if destination is None:
                raise ValueError(f"{path}:{line_number}: step is outside a scenario")
            if keyword in {"Given", "When", "Then"}:
                definition_keyword = keyword
            if definition_keyword is None:
                raise ValueError(
                    f"{path}:{line_number}: {keyword} has no preceding step type"
                )
            destination.append(Step(keyword, definition_keyword, text, line_number))
            continue
        if line.startswith("|") and line.endswith("|"):
            row = tuple(cell.strip() for cell in line[1:-1].split("|"))
            if reading_examples:
                examples.append(row)
                continue
            if destination is None or not destination:
                raise ValueError(f"{path}:{line_number}: table has no preceding step")
            destination[-1] = replace(
                destination[-1], table=(*destination[-1].table, row)
            )
            continue
        if line.endswith(":"):
            raise ValueError(f"{path}:{line_number}: unsupported section: {line}")
        if destination is not None:
            raise ValueError(
                f"{path}:{line_number}: unrecognized scenario line: {line}"
            )

    append_scenario()
    if not feature_name:
        raise ValueError(f"{path}: missing Feature declaration")
    if not templates:
        raise ValueError(f"{path}: no scenarios")
    scenarios = tuple(
        scenario for template in templates for scenario in expand_scenario(template)
    )
    return Feature(feature_name, tuple(background), tuple(scenarios))


def expand_scenario(template: ScenarioTemplate) -> tuple[Scenario, ...]:
    if not template.outline:
        if template.examples:
            raise ValueError(f"Scenario has unexpected examples: {template.name}")
        return (Scenario(template.name, template.steps),)
    if not template.examples:
        raise ValueError(f"Scenario Outline has no examples: {template.name}")

    return tuple(
        Scenario(
            substitute_examples(template.name, values),
            tuple(
                replace(
                    step,
                    text=substitute_examples(step.text, values),
                    table=tuple(
                        tuple(substitute_examples(cell, values) for cell in row)
                        for row in step.table
                    ),
                )
                for step in template.steps
            ),
        )
        for values in table_records(template.examples)
    )


def substitute_examples(text: str, values: dict[str, str]) -> str:
    def replace_placeholder(match: re.Match[str]) -> str:
        name = match.group(1)
        if name not in values:
            raise ValueError(f"unknown example placeholder <{name}> in: {text}")
        return values[name]

    return re.sub(r"<([^>]+)>", replace_placeholder, text)


def table_records(table: Table) -> list[dict[str, str]]:
    if len(table) < 2:
        raise ValueError("expected a table header and at least one data row")
    header = table[0]
    if not all(header) or len(set(header)) != len(header):
        raise ValueError(f"invalid table header: {header}")
    if any(len(row) != len(header) for row in table[1:]):
        raise ValueError(f"table row does not match header: {table}")
    return [dict(zip(header, row)) for row in table[1:]]


def collect_scenarios(feature_root: Path) -> tuple[ScenarioCase, ...]:
    paths = sorted(feature_root.glob("*.feature"))
    if not paths:
        raise ValueError(f"no feature files found in {feature_root}")
    return tuple(
        ScenarioCase(path, feature, scenario)
        for path in paths
        for feature in (parse_feature(path),)
        for scenario in feature.scenarios
    )


def run_scenario(case: ScenarioCase, context: Any) -> None:
    for current_step in (*case.feature.background, *case.scenario.steps):
        definition = _STEP_DEFINITIONS.get(
            (current_step.definition_keyword, current_step.text)
        )
        if definition is None:
            raise ValueError(
                f"{case.path}:{current_step.line}: undefined "
                f"{current_step.definition_keyword} step: {current_step.text}"
            )
        try:
            if current_step.table:
                definition(context, current_step.table)
            else:
                definition(context)
        except Exception as error:
            location = (
                f"feature step: {case.path}:{current_step.line} "
                f"({case.scenario.name}: {current_step.text})"
            )
            if hasattr(error, "add_note"):
                error.add_note(location)
            else:
                print(location, flush=True)
            raise
