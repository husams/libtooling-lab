"""Discriminated source selections and unambiguous symbol references."""

from dataclasses import dataclass
from typing import Literal


@dataclass(frozen=True, slots=True)
class FileReference:
    path: str
    repository: str | None = None
    clone: str | None = None
    component: str | None = None


@dataclass(frozen=True, slots=True)
class FileIdentity:
    file_id: str


@dataclass(frozen=True, slots=True)
class FileSelection:
    files: tuple[FileReference | FileIdentity, ...] | list[FileReference | FileIdentity]
    type: Literal["files"] = "files"


@dataclass(frozen=True, slots=True)
class DirectorySelection:
    path: str
    repository: str | None = None
    type: Literal["directory"] = "directory"


@dataclass(frozen=True, slots=True)
class ComponentSelection:
    component: str
    repository: str | None = None
    type: Literal["component"] = "component"


@dataclass(frozen=True, slots=True)
class RepositorySelection:
    repository: str
    type: Literal["repository"] = "repository"


@dataclass(frozen=True, slots=True)
class AllSelection:
    type: Literal["all"] = "all"


Selection = (
    FileSelection
    | DirectorySelection
    | ComponentSelection
    | RepositorySelection
    | AllSelection
)


ScanSelection = DirectorySelection | RepositorySelection | AllSelection


@dataclass(frozen=True, slots=True)
class SymbolReference:
    qualified_name: str | None = None
    usr: str | None = None
    symbol_id: str | None = None
    repository: str | None = None


@dataclass(frozen=True, slots=True)
class DeclarationLocation:
    path: str
    line: int
    column: int | None = None


@dataclass(frozen=True, slots=True)
class VariableReference:
    name: str
    declaration: DeclarationLocation | None = None
