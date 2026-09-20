"""Typed catalog resources, shared by synchronous and asynchronous clients."""

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class NewClone:
    path: str
    label: str | None = None
    id: str | None = None


@dataclass(frozen=True, slots=True)
class Clone:
    id: str
    path: str
    label: str | None = None


@dataclass(frozen=True, slots=True)
class Repository:
    id: str
    name: str
    kind: str
    remote_url: str | None
    active_clone_id: str | None
    clones: tuple[Clone, ...]
    component_count: int
    source_count: int
    indexed_source_count: int


@dataclass(frozen=True, slots=True)
class CompilationCommand:
    driver: str
    working_directory: str
    arguments: tuple[str, ...] | list[str]


@dataclass(frozen=True, slots=True)
class File:
    id: str
    path: str
    name: str
    directory_id: str
    component_id: str
    component: str
    repository_id: str | None
    compilation_command: CompilationCommand | None
    indexed: bool


@dataclass(frozen=True, slots=True)
class Component:
    id: str
    name: str
    path: str
    kind: str
    version: str | None
    repository_id: str | None
    repository: str | None
    file_count: int


@dataclass(frozen=True, slots=True)
class Directory:
    id: str
    component_id: str
    component: str
    path: str
    file_count: int
