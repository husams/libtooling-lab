"""Structured extraction outcomes and compiler diagnostics."""

from dataclasses import dataclass, field
from typing import Literal


@dataclass(frozen=True, slots=True)
class Diagnostic:
    severity: str
    message: str
    file: str
    line: int
    column: int


@dataclass(frozen=True, slots=True)
class FileExtraction:
    file_id: str
    path: str
    symbol_count: int


@dataclass(frozen=True, slots=True)
class AnalysisSummary:
    files_selected: int
    files_processed: int
    files_skipped: int
    files_failed: int
    coverage: Literal["complete", "partial"]
    index_revision: str | None
    diagnostics: tuple[Diagnostic, ...] | None = field(default=None, kw_only=True)


@dataclass(frozen=True, slots=True)
class ExtractionResult(AnalysisSummary):
    symbols_written: int
    files: tuple[FileExtraction, ...] | None = None


@dataclass(frozen=True, slots=True)
class DependencyEdge:
    source_file_id: str
    destination_file_id: str
    source_path: str
    destination_path: str


@dataclass(frozen=True, slots=True)
class DependencyResult(AnalysisSummary):
    edge_count: int
    edges: tuple[DependencyEdge, ...] | None = None
