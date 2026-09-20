"""Compilation discovery and traversal outcomes, including skipped symlinks."""

from dataclasses import dataclass
from typing import Literal

from .analysis_models import Diagnostic


@dataclass(frozen=True, slots=True)
class ImportedDatabase:
    path: str
    files_registered: int


@dataclass(frozen=True, slots=True)
class DiscoveredDatabase:
    path: str


@dataclass(frozen=True, slots=True)
class ImportResult:
    compilation_databases: int
    files_registered: int
    index_revision: str | None
    databases: tuple[ImportedDatabase, ...] | None = None
    diagnostics: tuple[Diagnostic, ...] | None = None


@dataclass(frozen=True, slots=True)
class ScanWarning:
    code: str
    severity: Literal["warning"]
    path: str
    target: str | None
    message: str
    action: Literal["skipped"]


@dataclass(frozen=True, slots=True)
class ScanResult:
    directories_scanned: int
    compilation_databases: int
    warning_count: int
    coverage: Literal["complete", "partial"]
    databases: tuple[DiscoveredDatabase, ...] | None = None
    warnings: tuple[ScanWarning, ...] | None = None
    diagnostics: tuple[Diagnostic, ...] | None = None
