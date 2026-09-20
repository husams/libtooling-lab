"""Watcher state and replaceable monitoring settings."""

from dataclasses import dataclass
from typing import Literal

from .discovery_models import ScanWarning


@dataclass(frozen=True, slots=True)
class WatchedClone:
    repository_id: str
    repository: str
    clone_id: str
    label: str
    path: str
    active: bool
    excluded: str


@dataclass(frozen=True, slots=True)
class WatcherSettings:
    enabled: bool
    debounce_ms: int
    exclude_repositories: tuple[str, ...] | list[str]
    exclude_clones: tuple[str, ...] | list[str]
    exclude_directories: tuple[str, ...] | list[str]
    exclude_patterns: tuple[str, ...] | list[str]


@dataclass(frozen=True, slots=True)
class WatcherStatus:
    enabled: bool
    running: bool
    active: bool
    pending: bool
    scanning: bool
    ready: bool
    source: str
    clones: tuple[WatchedClone, ...]
    notices: tuple[str, ...]
    directories: tuple[str, ...]
    events: int
    cycles: int
    failures: int
    overflows: int
    last_error: str
    latest_jobs: tuple[str, ...]
    import_mode: Literal["stored_commands", "reimport"]
    backend: Literal["inotify", "unsupported"]
    watched_directories: int
    warnings: tuple[ScanWarning, ...] = ()
    notice: str | None = None
    resumed: bool = False
