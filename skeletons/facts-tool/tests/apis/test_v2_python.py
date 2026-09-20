"""Public sync/async SDK exercises native typed resources and lazy job results."""

import asyncio
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python/src"))
from facts_tool.rest import (
    AsyncClient,
    Client,
    CompilationCommand,
    FileIdentity,
    FileSelection,
    RepositorySelection,
)


@pytest.mark.parametrize("asynchronous", [False, True])
def test_python_resource_workflow(domain_server, asynchronous):
    async def scenario():
        api = (AsyncClient if asynchronous else Client)(
            f"http://127.0.0.1:{domain_server.api.port}"
        )

        async def invoke(method, *args, **kwargs):
            value = method(*args, **kwargs)
            return await value if asynchronous else value

        try:
            assert (await invoke(api.server.health)).status == "ok"
            assert (await invoke(api.server.settings)).timeout_seconds > 0
            repos = await invoke(api.repositories.list().collect)
            assert {r.name for r in repos} == {"alpha", "beta"}
            prefixed = await invoke(api.symbols.find("alpha::ans").collect)
            assert any(s.qualified_name == "alpha::answer" for s in prefixed)
            assert (
                await invoke(api.symbols.find("alpha::ans", match="exact").collect)
                == []
            )
            files = await invoke(api.files.list(repository="alpha").collect)
            source = next(f for f in files if f.compilation_command is not None)
            command = source.compilation_command
            updated = await invoke(
                api.files.update,
                source.id,
                compilation_command=CompilationCommand(
                    command.driver,
                    command.working_directory,
                    [*command.arguments, "-DV2_PYTHON=1"],
                ),
            )
            assert not updated.indexed
            selection = FileSelection([FileIdentity(source.id)])
            extraction = await invoke(
                api.extractions.create, selection=selection, force=True
            )
            summary = await invoke(extraction.wait, timeout=30)
            assert summary.files_processed == 1 and summary.index_revision
            assert summary.files is None  # job GET never downloads the record arrays
            rows = await invoke(api.extractions.results(extraction.id).collect)
            assert rows[0].file_id == source.id
            match = await invoke(
                api.matches.create,
                selection=selection,
                expression='functionDecl(hasName("alpha::answer")).bind("custom")',
            )
            assert (await invoke(match.wait, timeout=30)).match_count >= 1
            matched = await invoke(api.matches.results(match.id).collect)
            assert matched[0].bindings["custom"].node_kind
            dependency = await invoke(api.dependencies.create, selection=selection)
            assert (await invoke(dependency.wait, timeout=30)).edge_count >= 1
            assert await invoke(api.dependencies.results(dependency.id).collect)
            imported = await invoke(api.imports.create, repository="alpha")
            imported_summary = await invoke(imported.wait, timeout=30)
            assert (
                imported_summary.files_registered == 0
                and imported_summary.databases is None
            )
            assert await invoke(api.imports.results(imported.id).collect)
            persisted = await invoke(api.files.get, source.id)
            assert "-DV2_PYTHON=1" in persisted.compilation_command.arguments
            scan = await invoke(
                api.scans.create, selection=RepositorySelection("alpha")
            )
            assert (await invoke(scan.wait, timeout=30)).warning_count == 0
            assert await invoke(api.scans.warnings(scan.id).collect) == []
            retained = await invoke(match.cancel)
            assert retained.state == "succeeded"
        finally:
            await api.aclose() if asynchronous else api.close()

    asyncio.run(scenario())
