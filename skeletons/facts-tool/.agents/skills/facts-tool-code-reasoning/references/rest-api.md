# Python resource API recipes

Contents: [setup](#server-setup), [catalog](#repositories-and-compilation-settings),
[discovery](#manual-discovery-and-import), [analysis](#extraction-and-dependencies),
[operations](#watcher-index-and-server), [API map](#resource-map).

Use these recipes inside an open `facts_tool.rest.Client` context from
[agent workflows](agent-workflows.md). All paths refer to the server host;
relative source paths resolve in the selected active clone/component.
Database filenames and CLI option arrays do not belong in resource requests.

## Server setup

Reuse a running server. If deployment is part of the task and none exists, the
native launcher is the exception to the Python-only analysis workflow:

```sh
facts-tool serve --server-config /workspace/server.yaml --host 127.0.0.1 --port 0
```

Use the actual saved/reported listener URL. Server startup discovers its own
configuration and database paths; provide `--config FILE` only to select
specific existing CLI defaults. Keep server deployment details in the
[deployment guide](../../../../docs/user-guide/09-rest-api/06-deployment.md).
Install the Python distribution with its HTTP extra:

```sh
python -m pip install 'facts-tool-query[rest]'
```

## Repositories and compilation settings

List existing registrations before creating one. For a new repository:

```python
from facts_tool.rest import NewClone

repo = client.repositories.create(
    name="example",
    clones=[NewClone(label="main", path="/workspace/example")],
)
repo = client.repositories.wait_until_ready(repo.id, timeout=120)
```

When monitoring is enabled, registration/startup discovers real compilation
databases, imports commands, extracts sources, and publishes the global index.
The build system must generate `compile_commands.json`; no analysis can
supply missing compiler configuration by guessing.

Clone registration and activation are repository updates, not separate APIs:

```python
repo = client.repositories.update(
    repo.id,
    clones=[*repo.clones, NewClone(label="work", path="/workspace/example-work")],
)
work_clone = next(clone for clone in repo.clones if clone.label == "work")
repo = client.repositories.update(repo.id, active_clone_id=work_clone.id)
repo = client.repositories.wait_until_ready(repo.id, timeout=120)
```

Use returned IDs. A supplied clone list replaces the collection; retain existing
`Clone` entries to keep their identities. Remove an active clone only while
selecting another in the same update. Omitted fields are unchanged;
`remote_url=None` explicitly clears that field.

Inspect files with `client.files.list(repository="example")` and
`client.files.get(file_id)`. A header can have `compilation_command=None`
and use an includer's context. Update settings on the file itself:

```python
from facts_tool.rest import CompilationCommand

# file_id is the actual ID of the file being changed.
source = client.files.get(file_id)
source = client.files.update(
    source.id,
    compilation_command=CompilationCommand(
        driver="clang++",
        working_directory="/workspace/example/build",
        arguments=["-std=c++23", "-I../include"],
    ),
)
```

Use the file's real, complete compiler settings in place of these examples:
the supplied command replaces its old settings and invalidates affected caches.
Automatic reimports preserve the explicit override. `arguments` contains
compiler options, not facts-tool CLI arguments.
Create a missing file with `client.files.create(path=...,
compilation_command=...)` only when its real command is known.

Catalog `.delete(id, cascade=False)` unregisters a resource; it does not delete
source files. Use `cascade=True` only when removing dependent registrations
is intended. Do not delete registrations as an analysis shortcut.

## Manual discovery and import

Use automatic monitoring by default. For an explicitly selected manual refresh:

```python
from facts_tool.rest import RepositorySelection

scan = client.scans.create(selection=RepositorySelection(repository="example"))
scan_summary = scan.wait(timeout=120)
for warning in client.scans.warnings(scan.id):
    print(warning)
for database in client.scans.databases(scan.id):
    print(database)

import_job = client.imports.create(
    repository="example",
    compilation_database="/workspace/example/build/compile_commands.json",
)
import_summary = import_job.wait(timeout=120)
for database in client.imports.results(import_job.id):
    print(database)
```

Imports can instead use `selection=RepositorySelection(...)`,
`DirectorySelection(...)`, or `AllSelection()` to discover databases.
A scan/import result is not extraction completion. If automatic processing is
disabled, submit targeted extraction next and inspect its publication result.
Read warnings/diagnostics, including filesystem and missing-context failures.

## Extraction and dependencies

```python
from facts_tool.rest import FileIdentity, FileSelection

selection = FileSelection(files=[FileIdentity(file_id=source.id)])
extraction = client.extractions.create(selection=selection, force=False)
summary = extraction.wait(timeout=120)
print(summary.coverage, summary.files_processed, summary.files_skipped)
for row in client.extractions.results(extraction.id):
    print(row.path, row.symbol_count)

dependency_job = client.dependencies.create(selection=selection)
dependency_summary = dependency_job.wait(timeout=120)
for edge in client.dependencies.results(dependency_job.id):
    print(edge.source_path, edge.destination_path)
```

Use `client.dependencies.create`; the callable
`client.dependencies(file)` is a legacy v1 interface.

Use a selection that matches the requested scope:

| Python model | Scope |
| --- | --- |
| `FileSelection([FileReference(path=..., repository=...)])` | Specific paths, optionally narrowed by clone/component |
| `FileSelection([FileIdentity(file_id=...)])` | Returned registered file IDs |
| `DirectorySelection(path=..., repository=...)` | A directory |
| `ComponentSelection(component=..., repository=...)` | A component |
| `RepositorySelection(repository=...)` | A repository |
| `AllSelection()` | All registered sources, explicitly |

Extraction, match, and dependency jobs require a selection. Variable flow accepts
an optional selection; include relevant caller/callee TUs when restricting it.
Scan/import discovery accepts directory, repository, or all selections only.

## Watcher, index, and server

Inspect `client.watcher.status()`, `client.watcher.settings()`,
`client.index.status()`, and `client.server.settings()` before changing
operational settings. For an intended partial watcher update:

```python
settings = client.watcher.update_settings(debounce_ms=500)
```

`update_settings` uses PATCH; omitted fields remain unchanged and supplied
exclusion lists replace their fields. `replace_settings(WatcherSettings(...))`
uses PUT and requires the full settings object. Do not disable monitoring or
erase exclusions as a routine troubleshooting step.

An explicitly needed index rebuild uses `client.index.create().wait()`.
It rebuilds discovery over stored facts; it is not source extraction.
`client.server.shutdown()` stops the shared server; call it only when shutdown
is part of the requested work.

## Resource map

Let the wrapper encode routes and payloads. V2 uses GET for reads, POST for
resource/job creation, PATCH for selected updates, PUT for complete settings,
and DELETE for unregistering/cancelling. IDs and version are in URLs; GET and
DELETE have no JSON body.

| Task | Python namespace and methods |
| --- | --- |
| Repository and clone management | `repositories.list/get/create/update/delete/wait_until_ready` |
| Components | `components.list/get/create/update/delete` |
| Directories | `directories.list/get/delete` |
| Files and compiler settings | `files.list/get/create/update/delete` |
| Global symbols | `symbols.find/get/occurrences/relations` |
| Extraction | `extractions.create/get/list/cancel/results/diagnostics` |
| AST matching | `matches.create/get/list/cancel/results/diagnostics` |
| Compilation import | `imports.create/get/list/cancel/results/diagnostics` |
| Include dependencies | `dependencies.create/get/list/cancel/results/diagnostics` |
| Call graphs and paths | `callgraphs.create/get/list/cancel/nodes/edges/paths/frontier/diagnostics` |
| Variable flow | `variable_flow.create/get/list/cancel/nodes/edges/boundaries/diagnostics` |
| Directory scans | `scans.create/get/list/cancel/warnings/databases/diagnostics` |
| Global index | `index.status/create/get/list/cancel/results` |
| Watcher | `watcher.status/settings/update_settings/replace_settings` |
| Server | `server.health/readiness/settings/shutdown` |

See the [resource contract](../../../../docs/user-guide/09-rest-api/02-requests-and-jobs.md)
and [Python guide](../../../../python/docs/rest-v2.md) for schemas and semantics.
