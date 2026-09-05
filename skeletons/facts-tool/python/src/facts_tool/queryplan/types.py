from dataclasses import dataclass
from enum import StrEnum
from typing import Any


class UnknownPolicy(StrEnum):
    EXCLUDE = "exclude"
    INCLUDE = "include"
    ERROR = "error"


class TraversalMode(StrEnum):
    STATIC = "static"
    DEVIRTUALIZED = "devirtualized"


@dataclass(frozen=True)
class Pred:
    op: str
    field: str = ""
    value: Any = None
    kids: tuple["Pred", ...] = ()
    relation: str = ""
    target: "Pred | None" = None
    min_depth: int = 1
    max_depth: int = 1
    threshold: int = 0
    inbound: bool = False


@dataclass(frozen=True)
class TargetSet:
    kind: str
    refs: tuple[str, ...]


@dataclass(frozen=True)
class Source:
    kind: str
    ref: str = ""


@dataclass(frozen=True)
class Stage:
    op: str
    pred: Pred | None = None
    level: str = "symbol"
    relation: str = ""
    mode: str = "static"
    min_depth: int = 1
    max_depth: int = 1
    operand: "Plan | None" = None
    fields: tuple[str, ...] = ()
    n: int = 0
    unknown: str = "exclude"
    inbound: bool = False


@dataclass(frozen=True)
class Plan:
    source: Source
    stages: tuple[Stage, ...] = ()


@dataclass(frozen=True)
class Query:
    plan: Plan

    def __or__(self, stage: Stage) -> "Query":
        return Query(Plan(self.plan.source, self.plan.stages + (stage,)))

    def to_plan(self) -> Plan:
        return self.plan
