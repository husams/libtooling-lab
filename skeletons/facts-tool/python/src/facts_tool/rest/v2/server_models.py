"""Typed server health and configuration snapshots."""

from dataclasses import dataclass
from typing import Literal

from .watch_models import WatcherSettings


@dataclass(frozen=True, slots=True)
class Health:
    status: Literal["ok"]


@dataclass(frozen=True, slots=True)
class Readiness:
    ready: bool
    index_ready: bool
    watcher_ready: bool


@dataclass(frozen=True, slots=True)
class ServerSettings:
    watcher: WatcherSettings
    timeout_seconds: int
    authenticated: bool


@dataclass(frozen=True, slots=True)
class Shutdown:
    status: Literal["stopping"]
