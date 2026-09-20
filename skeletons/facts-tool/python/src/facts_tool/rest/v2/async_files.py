"""Files own their compilation settings; components own directory metadata."""

import httpx

from .async_resource import MutableResource
from .catalog_models import CompilationCommand, Component, Directory, File
from .codec import UNSET, Unset


class Files(MutableResource[File]):
    def __init__(self, http: httpx.AsyncClient) -> None:
        super().__init__(http, "files", File)

    async def create(
        self, *, path: str, compilation_command: CompilationCommand
    ) -> File:
        return await self._create(
            {"path": path, "compilation_command": compilation_command}
        )

    async def update(
        self, identifier: str, *, compilation_command: CompilationCommand
    ) -> File:
        return await self._update(
            identifier, {"compilation_command": compilation_command}
        )


class Components(MutableResource[Component]):
    def __init__(self, http: httpx.AsyncClient) -> None:
        super().__init__(http, "components", Component)

    async def create(
        self,
        *,
        name: str,
        path: str,
        kind: str = "repo",
        version: str | None = None,
        repository: str | None = None,
    ) -> Component:
        return await self._create(
            {
                k: v
                for k, v in {
                    "name": name,
                    "path": path,
                    "kind": kind,
                    "version": version,
                    "repository": repository,
                }.items()
                if v is not None
            }
        )

    async def update(
        self,
        identifier: str,
        *,
        name: str | Unset = UNSET,
        version: str | None | Unset = UNSET,
    ) -> Component:
        return await self._update(identifier, {"name": name, "version": version})


class Directories(MutableResource[Directory]):
    def __init__(self, http: httpx.AsyncClient) -> None:
        super().__init__(http, "directories", Directory)
