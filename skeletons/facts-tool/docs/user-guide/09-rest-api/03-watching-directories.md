# Automatic reimport and indexing

Linux servers can recursively monitor source directories using inotify:

```sh
facts-tool serve --daemon --server-config /workspace/server.yaml \
  --conf /workspace/project.db --config /workspace/defaults.yaml \
  --watch /workspace/repo-a --watch /workspace/repo-b
```

The directory list is persisted with the host and allocated port. A later
`serve --server-config /workspace/server.yaml` loads it automatically. Supply a
new `--watch` list to replace it, or `--no-watch` to disable monitoring.
Inotify watching requires Linux; foreground and daemon HTTP operation use POSIX
process facilities. A watch request on an unsupported platform fails explicitly.

## What happens after an edit

Close-after-write, creation, deletion and move events for C/C++ source files,
headers and `compile_commands.json` trigger a debounced refresh. Atomic editor
saves and newly created nested directories are handled. Changes that occur
during an active refresh are coalesced into a later refresh.

The watcher finds compilation databases recursively below every watched root.
For each refresh it reimports each discovered database, then runs
`extract --force` using the shared project and CLI defaults. Both watcher commands
also receive `--no-ast-cache`, discarding the project's prior cache metadata and
bypassing the commit-keyed AST and dependency cache so edits at an unchanged Git
commit are reparsed. This also prevents later API matcher jobs from reusing an
older snapshot. Physical cache files may remain on disk; invalidated metadata
prevents their reuse.
Import must succeed
before extraction starts. Both steps are normal jobs, sharing the API's serialized
worker queue. Forced extraction covers uncommitted edits even when the repository
commit has not changed. It currently processes the selected stored source set,
not only the individual file that emitted the event.

Watching does not initiate an import on startup. Import and extract initially
through the CLI or REST endpoints; subsequent relevant filesystem events refresh
the index. A source file newly added to a compilation database is imported on the
next refresh. Updating the build system's compilation database remains the build
system's responsibility.

Without a discovered compilation database, the watcher reruns forced extraction
using stored compilation commands. In this mode it reports
`import_mode: "stored_commands"` and a notice. Register new translation units
explicitly or configure automatic import arguments.

## Explicit automatic command arguments

If compilation databases live outside the watched roots, select one explicitly:

```sh
facts-tool serve --server-config /workspace/server.yaml \
  --watch /workspace/project/src \
  --import-arg=-p --import-arg=/workspace/project/build \
  --extract-arg=-o --extract-arg=/workspace/facts.db
```

`import_arguments` replaces automatic compilation-database discovery for import.
`extract_arguments` is passed to `extract`; the watcher always enforces `--force`
and disables AST caching for both commands.
Do not include the executable or the `import`/`extract` command token. The equals
form allows a token beginning with a dash to be passed as a value.

## Check progress and failures

```sh
curl -sS "$API/v1/watch"
```

| Field | Meaning |
|---|---|
| `enabled`, `running` | Watch configuration and event processing state |
| `ready`, `scanning` | Directory registration readiness and rescan activity |
| `active`, `pending` | Refresh in progress and additional changes waiting |
| `directories`, `watched_directories` | Configured roots and active directory-watch count |
| `events`, `cycles` | Relevant event count and started refresh count |
| `failures`, `last_error` | Failed refresh count and latest refresh error |
| `overflows` | Inotify event queue overflow count |
| `latest_jobs` | IDs to inspect using `GET /v1/jobs/{id}` |
| `import_mode`, `backend` | Import strategy and operating-system watcher |

A failed import or extraction is visible in `last_error` and the corresponding
job's `stderr` and `exit_code`. The server keeps handling HTTP. An inotify overflow
requests a rescan and refresh rather than silently assuming the index is current.
Source deletion triggers refresh, but removal of stale compilation-database
entries and project catalog registrations follows the existing CLI semantics.

To prevent indexing feedback loops, watches ignore database, AST-cache and object
outputs, SQLite sidecar files, the server configuration and its sibling log/PID
files, and directories named `.git`, `.cache`, `.facts`, `.facts-tool`, `.deps` or
`node_modules`. Symlinked directories are not traversed.
