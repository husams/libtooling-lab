# Local facts-tool server

The validated local instance uses the server-capable source snapshot in
`.server-runtime/source`, built as `.server-runtime/build/facts-tool`, and the
matching Python SDK in `.server-runtime/venv`. The managed checkout is this
directory, not the nested server source snapshot. Server implementation changes
are tracked directly in `src/apis` and the Python SDK; the ignored snapshot is
an instance build artifact. These instance scripts require an existing build and
Python environment. Use `FACTS_SERVER_BINARY`, `FACTS_SERVER_PYTHON` and
`FACTS_SERVER_RUNTIME` to select another installation for start/stop/status.

```bash
./scripts/start-server.sh
./scripts/stop-server.sh
.server-runtime/venv/bin/python scripts/server-control.py status
```

Start is idempotent. Stop requests graceful shutdown and waits for the instance
lock to be released; it does not kill an unrelated process from a stale PID.
An active native analysis may delay shutdown. The default wait is 300 seconds;
use `./scripts/stop-server.sh --timeout 3600` for a long analysis.
The daemon survives the launching shell. These scripts do not install a boot
service or automatically restart a crashed process.

The listener is local to this machine. Read the actual endpoint from the start
or status output; the port is allocated on first startup and saved. HTTP
readiness is separate from completed extraction and index publication.

| Artifact | Location |
|---|---|
| Server configuration | `.server-runtime/server.yaml` |
| Extraction defaults | `.server-runtime/defaults.yaml` |
| Project catalog and global index | `.server-runtime/project.db` |
| Facts databases | `.server-runtime/facts/` |
| Structured server log | `.server-runtime/logs/server.jsonl` |
| PID and instance lock | `.server-runtime/server.yaml.pid` |
| Test results and diagnostic bundles | `.server-runtime/evidence/` |

Server YAML edits take effect after restart. The watcher monitors eligible
repository sources and uses the compilation database selected in
`import_arguments`. Build outputs, nested server sources, dependency trees, and
virtual environments are excluded. Update the compilation database through the
build system when adding a translation unit. Watcher changes to source/header
contents trigger import and extraction. Global index refresh alone reads facts;
it does not recompile source files.

## Python access and manual index refresh

Run Python with `.server-runtime/venv/bin/python` from the repository root:

```python
import yaml
from pathlib import Path
from facts_tool.rest import Client

config = yaml.safe_load(Path('.server-runtime/server.yaml').read_text())
url = f"http://{config['host']}:{config['port']}"
with Client(url, timeout=60) as client:
    print(client.server.readiness())
    print(client.watcher.status())
    print(client.index.status())
    for symbol in client.symbols.find('facts::', repository='facts-tool'):
        print(symbol.qualified_name, symbol.definition)
    job = client.index.create()
    print(job.id, job.wait(timeout=300))
```

For CLI extraction into an additional facts database, use the same catalog and
defaults as the server. The file must already have a registered compilation
command:

```bash
.server-runtime/build/facts-tool extract --force \
  --conf "$PWD/.server-runtime/project.db" \
  --config "$PWD/.server-runtime/defaults.yaml" \
  -o /absolute/path/to/additional-facts.db /absolute/path/to/source.cpp
```

Then submit `client.index.create()` and wait for success. Merely placing an
unregistered SQLite file in a directory does not identify its repository or
sources to the server. The global index uses the catalog's facts associations.

## Investigating failures

Save a failed job before restarting the server or allowing retained history to
expire:

```bash
.server-runtime/venv/bin/python scripts/inspect-server-job.py extract JOB_ID
```

The command writes a JSON bundle containing job metadata, structured error
details, and log records sharing that job ID. Supported families are `extract`,
`match`, `import`, `dependencies`, `callgraphs`, `variable-flow`, `scan`, and
`index`. Current-job records are filtered by creation time because IDs can be reused
after restart. An unavailable job still permits recovery of historical records
matching that ID; inspect their timestamps and operation fields.
Compiler failures should identify the source location, clone, compilation
command, and diagnostic. Inspect the error's `details` before changing flags.

After repairing the source or compilation settings:

```python
failed = client.extractions.get('JOB_ID')
retry = failed.retry()
print(retry.id, retry.wait(timeout=300))
```

Retry creates a new job and preserves the original failure. Retained jobs do
not survive restart; facts and the global index do. Watcher failures appear in
`client.watcher.status().last_error`; its `latest_jobs` are compatibility job
IDs readable through `client.legacy.get_job(id)`.

Logs append across restarts. Graceful shutdown drains accepted log records.
There is no built-in log rotation; monitor `.server-runtime/logs` disk usage.

## Repeating validation

```bash
./scripts/test-server.sh
```

The runner first checks Python client and analysis-reader behavior, then starts
isolated test servers covering authentication, lifecycle, API contracts,
resource CRUD, analysis, watchers, retry/cancellation, indexing, and logging.
Isolated API fixtures live outside the checkout so fixtures without Git
metadata do not inherit this repository's identity; their location is recorded
in `fixtures-location.txt`. The final suite runs typed clients against the configured
instance, including real source analysis and a CLI-created facts database.
It briefly disables watching while temporary registrations are tested, then
removes them and restores watcher settings. Run it when no other user is
changing this instance's configuration. Test sources and evidence are retained.
The script prints the report directory and returns nonzero for any failure.

## Batch failure collection

The rebuilt server and editable Python SDK support `continue_on_error=True` for
extraction, matching and dependencies. For example:

```python
from facts_tool.rest import Client, RepositorySelection

with Client('http://127.0.0.1:33431', timeout=60) as api:
    job = api.extractions.create(
        selection=RepositorySelection('facts-tool'),
        continue_on_error=True,
    )
    result = job.wait(timeout=600)
    print(job.id, job.state, result.coverage, result.files_failed)
    for failure in api.extractions.failed_files(job.id):
        print(failure.path, failure.error.message, failure.error.details)
```

Mixed outcomes have `state=succeeded`, `coverage=partial`, and a nonzero
`files_failed` count. If every selected file fails, the job is `failed` and
`wait()` raises `JobFailed`. Failed/cancelled batches retain their partial summary
and result collections when available. `files_not_attempted` distinguishes files
not reached from failed files; `files_skipped` means unchanged, valid input.
The default remains stop-on-first-error. Both sync and async clients support the
flag, failure pagination, status lookup and retry.

REST equivalents:

- `POST /api/v2/extract/job` with `selection` and `continue_on_error: true`.
- `GET /api/v2/extract/job/{id}` for state, counts, coverage and terminal error.
- `GET /api/v2/extract/job/{id}/results?collection=failed_files&limit=50`
  for file IDs, paths and full structured errors; follow `next_cursor`.
- `GET /api/v2/extract/job/{id}/results?collection=diagnostics` for compiler diagnostics.
- `POST /api/v2/index/job` with `{}` to rebuild the global index after CLI extraction;
  inspect `GET /api/v2/index/job/{id}` for its outcome.

Substitute `match` or `dependencies` for the other batch analyses. Each continued
failure logs `job.file_failed`, with `job.diagnostic` and chunked `job.context`
records sharing its job ID and file ID. `job.completed` logs summary counts and
coverage. The inspection script now exports failure records, diagnostics and
successful rows as well as metadata and correlated logs.

Cancellation and selection/global-index errors still stop a batch. Index refresh
remains transactional; it does not silently skip corrupt facts databases.
Legacy watcher command batches retain their existing stop-on-error behavior.
Retries rerun the original selection with current inputs and preserve the previous
job; submit an explicit file selection to retry only failed file IDs.

To repeat the controlled check on real repository files:

```bash
.server-runtime/venv/bin/python scripts/test-server-continuation.py
```

It temporarily injects missing-header compiler options for two registered sources,
verifies later-file processing, then restores the commands and re-extracts them.
It also checks async failure pagination, all-failed status, cancellation, retry,
logs and REST reindex after CLI extraction. Source contents are checked by hash.
