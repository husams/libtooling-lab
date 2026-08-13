from __future__ import annotations

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
class Feature:
    name: str
    background: tuple[Step, ...]
    scenarios: tuple[Scenario, ...]


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
    scenarios: list[Scenario] = []
    scenario_name: Optional[str] = None
    scenario_steps: list[Step] = []
    destination: Optional[list[Step]] = None
    definition_keyword: Optional[str] = None

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
            continue
        if line.startswith("Scenario:"):
            if scenario_name is not None:
                scenarios.append(Scenario(scenario_name, tuple(scenario_steps)))
            scenario_name = line.removeprefix("Scenario:").strip()
            scenario_steps = []
            destination = scenario_steps
            definition_keyword = None
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
            if destination is None or not destination:
                raise ValueError(f"{path}:{line_number}: table has no preceding step")
            row = tuple(cell.strip() for cell in line[1:-1].split("|"))
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

    if scenario_name is not None:
        scenarios.append(Scenario(scenario_name, tuple(scenario_steps)))
    if not feature_name:
        raise ValueError(f"{path}: missing Feature declaration")
    if not scenarios:
        raise ValueError(f"{path}: no scenarios")
    return Feature(feature_name, tuple(background), tuple(scenarios))


def table_records(table: Table) -> list[dict[str, str]]:
    if len(table) < 2:
        raise ValueError("expected a table header and at least one data row")
    header = table[0]
    if not all(header) or len(set(header)) != len(header):
        raise ValueError(f"invalid table header: {header}")
    if any(len(row) != len(header) for row in table[1:]):
        raise ValueError(f"table row does not match header: {table}")
    return [dict(zip(header, row)) for row in table[1:]]


def run_features(feature_root: Path, context_factory: Callable[[], Any]) -> None:
    paths = sorted(feature_root.glob("*.feature"))
    if not paths:
        raise ValueError(f"no feature files found in {feature_root}")
    for path in paths:
        _run_feature(path, parse_feature(path), context_factory)


def _run_feature(
    path: Path, feature: Feature, context_factory: Callable[[], Any]
) -> None:
    print(f"Feature: {feature.name}", flush=True)
    for scenario in feature.scenarios:
        context = context_factory()
        print(f"  Scenario: {scenario.name}", flush=True)
        for current_step in (*feature.background, *scenario.steps):
            print(f"    {current_step.keyword} {current_step.text}", flush=True)
            for row in current_step.table:
                print(f"      | {' | '.join(row)} |", flush=True)
            definition = _STEP_DEFINITIONS.get(
                (current_step.definition_keyword, current_step.text)
            )
            if definition is None:
                raise ValueError(
                    f"{path}:{current_step.line}: undefined "
                    f"{current_step.definition_keyword} step: {current_step.text}"
                )
            try:
                if current_step.table:
                    definition(context, current_step.table)
                else:
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
