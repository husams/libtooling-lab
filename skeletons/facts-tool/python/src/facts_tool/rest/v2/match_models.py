"""Matcher result bindings remain typed while binding names are unrestricted."""

from dataclasses import dataclass

from .analysis_models import AnalysisSummary


@dataclass(frozen=True, slots=True)
class MatchLocation:
    path: str
    line: int
    column: int
    offset: int


@dataclass(frozen=True, slots=True)
class MatchRange:
    path: str
    offset: int
    size: int


@dataclass(frozen=True, slots=True)
class MatchBinding:
    node_kind: str
    name: str | None
    usr: str | None
    location: MatchLocation | None
    range: MatchRange | None
    location_unavailable_reason: str | None


@dataclass(frozen=True, slots=True)
class MatchRow:
    translation_unit: str
    relation_kind: str | None
    bindings: dict[str, MatchBinding]


@dataclass(frozen=True, slots=True)
class MatchResult(AnalysisSummary):
    match_count: int
    matches: tuple[MatchRow, ...] | None = None


@dataclass(frozen=True, slots=True)
class MatcherBindings:
    source: str | None = None
    target: str | None = None
    site: str | None = None
    call: str | None = None
    callee: str | None = None
