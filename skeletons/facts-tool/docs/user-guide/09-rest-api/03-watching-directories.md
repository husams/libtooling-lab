# Repository monitoring, reimport and indexing

← [User guide index](../README.md) · [Table of contents](../toc.md)

On Linux, the server uses inotify to monitor the active clone of every repository
registered in its project database. Monitoring is enabled by default:

```sh
facts-tool serve --daemon --server-config /workspace/server.yaml \
  --conf /workspace/project.db --config /workspace/defaults.yaml
```

There is no separate directory list to maintain. Register repositories with
`repo add` or `import` and inspect them with `repo list` and `repo show`.
The running server follows repository registrations, removals and `repo switch`
changes automatically, checking the catalog every second. Only active clones
are monitored, matching the CLI's source-selection model; a registered inactive
clone becomes eligible when made active. Registry changes after startup also
trigger a filtered refresh, so edits
made before the new watches are attached are picked up. Unavailable clone paths
are reported in `notices` and retried while other clones continue to be monitored.
See [Repositories and projects](../02-projects-and-configuration/01-repositories-and-projects.md).

Use `--no-watch` to disable monitoring or `--watch` to enable it again. Both flags
persist `watch.enabled`; neither accepts a directory argument. The old
`watch_directories` YAML key is ignored and removed on the next successful save.
Inotify requires Linux. Monitoring defaults to disabled on other platforms;
explicitly enabling it there fails with an unsupported-platform error. HTTP
operation uses POSIX process facilities and remains available without monitoring.

## Exclude repositories, clones and paths

Put exclusions in the **server** YAML selected by `--server-config`, alongside
its saved host and port. Restart the server after editing this file:

```yaml
watch:
  enabled: true
  exclude_repositories: [vendor-library]
  exclude_clones: [engine:ci]
  exclude_directories:
    - generated
    - /workspace/engine/third_party
  exclude_patterns:
    - '*.generated.cpp'
    - 'scratch/**'
```

| Setting | Matching rule |
|---|---|
| `exclude_repositories` | Repository name or numeric repository ID |
| `exclude_clones` | Absolute clone path, label, `repository:label`, or numeric clone ID |
| `exclude_directories` | Directory and its descendants; relative to each clone or absolute |
| `exclude_patterns` | Additional Git ignore style patterns, relative to each clone |

An unqualified clone label matches every clone with that label; use
`repository:label` or an absolute path to target one. Quote numeric IDs when
writing YAML, for example `exclude_repositories: ['7']`. Exclusions affect both
event monitoring and the source files passed to automatic import and extraction.
An included source can still include an excluded header. Clang can read, register
and analyse that header as a dependency; exclusions select translation units and
watch paths, without changing normal include resolution or compiler options.

## Git ignore rules

Each clone's `.gitignore` rules apply automatically, including nested
`.gitignore` files and Git's negation rules. Tracked files remain eligible even
when a `.gitignore` pattern matches them, following Git's tracked-file semantics.
YAML exclusions are additional restrictions and can exclude tracked files too.
Registered directories without Git metadata also honor their `.gitignore` files;
the watcher does not create a `.git` directory.

Changes to ignore rules are picked up while the server runs. Ignored files do
not trigger refreshes and are not added to automatic source processing. This
includes ignored generated sources referenced by a compilation database. To
analyse them automatically, adjust their ignore rule and any YAML exclusion.

## What happens after an edit

Close-after-write, creation, deletion and move events for included C/C++ sources,
headers and `compile_commands.json` trigger a debounced refresh. Atomic editor
saves and newly created nested directories are handled. Changes during an active
refresh are coalesced into a later refresh.

The watcher discovers compilation databases below eligible clones. A refresh
reimports compilation commands for included sources, then runs `extract --force`
for included stored sources. Both steps use shared project and CLI defaults and
the API's serialized worker queue. Import must succeed before extraction starts.
The source set is filtered again for automatic jobs; an event in one included
file cannot cause an excluded repository, clone or source to be reprocessed.

Both commands receive `--no-ast-cache`, discarding prior project cache metadata
and bypassing commit-keyed AST and dependency caching. Uncommitted edits are
therefore reparsed even when the Git commit has not changed. This also prevents
later matcher jobs from reusing an older snapshot. Physical cache files can
remain; invalidated metadata prevents their reuse. Shared AST/cache and call-graph
entry metadata can be invalidated across the project for dependency correctness,
even though excluded translation units are not compiled or reindexed.

Startup establishes watches without indexing. Import and extract initially
through the CLI or REST endpoints. A new translation unit is imported on a
subsequent refresh when it appears in a compilation database and passes the
filters. Updating that database remains the build system's responsibility.
Without a discovered or explicitly selected compilation database, the watcher
extracts eligible sources using stored compilation commands and reports
`import_mode: "stored_commands"`.
Register new translation units explicitly or configure import arguments.

## Explicit automatic command arguments

Automatic discovery skips ignored directories, including a Git-ignored `build/`.
If the compilation database is there or outside the monitored clones, select it
explicitly:

```sh
facts-tool serve --server-config /workspace/server.yaml \
  --import-arg=-p --import-arg=/workspace/project-build \
  --extract-arg=-o --extract-arg=/workspace/facts.db
```

Explicit `-p` values replace automatic compilation-database discovery. The server
also watches each explicit database's parent for changes to `compile_commands.json`;
this allows an ignored or out-of-tree build directory to supply commands without
monitoring its other ignored files. `extract_arguments` supplies extraction
options. The watcher still restricts source processing to eligible active clones
and enforces `--force` for extraction
and `--no-ast-cache` for both commands. Do not include the executable or command
token. The equals form permits values beginning with a dash.

Automatic arguments accept `--config`, `--extra-arg` and verbosity options;
imports also accept `-p`/`--compilation-database` and `-f`/`--facts`, and extraction
accepts `-o`/`--output`. Positional sources, `--conf`/`-c`, clone identity overrides
and other unsupported options are rejected: the resolved project database and
filters determine source selection. Reimports pass `--existing-clone` internally
to preserve registered names, clone labels and component definitions.

## Check progress and failures

```sh
curl -sS "$API/v1/watch"
```

| Field | Meaning |
|---|---|
| `enabled`, `running` | Watch configuration and event processing state |
| `ready`, `scanning` | Directory registration readiness and rescan activity |
| `active`, `pending` | Refresh in progress and additional changes waiting |
| `source`, `project_database` | Root source (`project_database`) and resolved catalog path |
| `clones` | Registered clones with repository/clone IDs, names, paths, active flags and exclusions |
| `directories`, `watched_directories` | Eligible, available active clone roots and active directory-watch count |
| `notices` | Nonfatal monitoring notices, including unavailable clone paths |
| `events`, `cycles` | Relevant event count and started refresh count |
| `failures`, `last_error` | Failed refresh count and latest refresh error |
| `overflows` | Inotify event queue overflow count |
| `latest_jobs` | IDs to inspect using `GET /v1/jobs/{id}` |
| `import_mode`, `backend` | Import strategy and operating-system watcher |

`ready` stays false while `notices` reports an unavailable clone or compilation
directory. Other available clones continue to be monitored and HTTP remains
available. The watcher retries unavailable paths without requiring a restart.

A failed import or extraction is visible in `last_error` and its job's `stderr`
and `exit_code`; HTTP remains available. Inotify overflow requests a rescan and
refresh. Source deletion triggers refresh, but removal of stale compilation
commands and catalog entries follows the existing CLI semantics.

Database, AST-cache and object outputs, SQLite sidecar files, server configuration
and its sibling log/PID files are excluded to prevent feedback loops. `.git`
directories are excluded from source traversal; specific Git control files are
still watched to reload ignore rules and tracked-file state. Other directory
names, including `.cache`, `.facts`, `.facts-tool`, `.deps` and `node_modules`,
are excluded only when Git ignore rules or YAML exclusions match them. Symlinked
directories are not traversed.
