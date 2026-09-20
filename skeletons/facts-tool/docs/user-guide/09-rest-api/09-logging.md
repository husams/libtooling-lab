# Logging and verbosity

← [User guide index](../README.md) · [Table of contents](../toc.md)

Configure server logging independently of the listener configuration filename.
Use the YAML selected by `--server-config`:

```yaml
logging:
  file: /var/log/facts-tool/server.jsonl
  level: info
```

The server account must be able to create or append to the destination.
The same file setting works in foreground and daemon mode. Without `logging.file`,
foreground events go to stderr and daemon events go to `<server-config>.log`.
The resolved daemon destination is saved and reused on subsequent starts,
including foreground starts. Remove `logging.file` to restore mode defaults;
an empty file value is invalid.

## CLI options and precedence

```bash
facts-tool serve --server-config /workspace/server.yaml \
  --log-file /workspace/logs/server.jsonl --log-level debug
```

Explicit CLI options override saved YAML settings. Relative YAML file paths are
resolved against the server configuration's directory; relative `--log-file`
paths are resolved against the invocation directory, before any job working
directory change. Saved paths are absolute. Log files are appended to on restart.
Settings are persisted after successful startup; editing YAML requires a restart.

Choose a named level or its numeric verbosity alternative:

| Level | `-v N` or `--verbose N` | Events included |
|---|---|---|
| `off` | — | No routine events; fatal diagnostics still appear |
| `error` | `0` | Failures |
| `warning` | — | Warnings and errors |
| `info` | `1` | Normal lifecycle events, warnings and errors; default |
| `debug` | `2` | Request summaries plus info, warnings and errors |
| `trace` | `3` | Detailed HTTP I/O plus all other enabled events |

For example, `facts-tool serve -v 2` selects `debug`. YAML also accepts
`logging.verbosity: 2` as an alternative to `logging.level: debug`.
Use only one of those YAML keys, and only one of `--log-level` or `-v` on the CLI.
The saved configuration always uses the canonical named `logging.level`.

Server verbosity controls the REST service. Typed resource operations return
structured results and errors through `GET /v1/jobs/{id}`; clients do not submit
per-command verbosity or parse terminal output. Server logs are independent of
those operation results. Only deprecated command compatibility jobs retain
captured stdout/stderr and CLI verbosity arguments. Watcher command verbosity
belongs in `import_arguments` or `extract_arguments`.

## Structured events and operation

Each JSON line has `timestamp` (Unix epoch milliseconds), `level`, `event`,
and a `fields` object. Job events include `fields.job_id`; use that identifier
to retrieve the complete job through REST.

| Event | Level | Meaning |
|---|---|---|
| `server.ready`, `server.stopping`, `server.stopped` | info | Server lifecycle |
| `server.failed` | error | Server failure |
| `job.accepted`, `job.started` | info | Command queue and worker progress |
| `job.completed` | info, warning or error | Completion; timeout warns, failure errors |
| `queue.rejected` | warning | Job queue refused submission |
| `index.completed`, `index.failed` | info, error | Background global-index publication or failure |
| `http.response` | debug | Method, route template, status and processing duration |
| `http.read`, `http.write` | trace | HTTP I/O byte counts and failure status |
| `http.invalid`, `http.failed` | warning, error | Invalid HTTP or handler failure |
| `watch.started`, `watch.stopped` | info | Repository monitoring lifecycle |
| `watch.scanned`, `watch.cycle.started`, `watch.cycle.completed` | debug | Scan and refresh progress; failed cycle completion warns |
| `watch.scan`, `watch.event` | trace | Scan triggers and filesystem event metadata |
| `watch.failed`, `watch.overflow` | warning | Monitoring failure or inotify overflow |

HTTP events use route templates. Tokens, authorization headers, request bodies,
query values and raw CLI arguments are not logged. Inspect job output separately
when diagnosing a command failure.

File writes run on a dedicated worker with a 4,096-record queue. Overflow drops
new records and reports their count in `logger.dropped`. Oversized event fields
become `{"truncated":true}`. Graceful shutdown drains accepted records; disk
writes stay off the HTTP event loop. Log rotation is not built in. A write failure
reports a diagnostic on stderr and disables further log writes.

Readiness and fatal diagnostics remain visible even at level `off`.
`facts-tool: server ready at HOST:PORT` goes to stdout; the daemon parent prints it
after startup succeeds. Fatal JSON diagnostics go to stderr, redirected to the
daemon log. An unusable log destination fails startup without announcing readiness.

For systemd journal configuration and file examples, see
[Deployment and operation](06-deployment.md). For ping, job polling and watcher
state, see [Requests and asynchronous jobs](02-requests-and-jobs.md).
