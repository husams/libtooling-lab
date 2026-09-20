# Running facts-tool as a server

← [User guide index](../README.md) · [Table of contents](../toc.md)

`facts-tool` remains a CLI. The additional `serve` command exposes symbol lookup
and source analysis over HTTP for applications and agents. The server resolves
project and facts storage; clients identify symbols or registered files.
Install it using [REST installation](05-installation.md); see
[Deployment and operation](06-deployment.md) for foreground, daemon and Linux
systemd service setup.

```sh
facts-tool serve --server-config /workspace/server.yaml \
  --host 127.0.0.1 --port 0 \
  --conf /workspace/project.db --config /workspace/defaults.yaml
```

`--config` is the usual CLI defaults YAML. `--server-config` is a separate file
for listener, logging, watcher and worker settings. Both project options are optional:
ordinary configuration discovery selects the server-owned project database.
Client resource requests never override its database paths.

Port `0` asks the operating system to allocate an available port. After binding,
the server writes its actual host and port to the server configuration and prints
`facts-tool: server ready at HOST:PORT`. Read that port before making requests.
On restart, the saved port is reused when available; if it has been taken and
no explicit `--port` was supplied, a new available port is saved automatically.
An occupied port supplied explicitly is a startup error.

The server creates its project schema and indexes known existing facts files on a
background worker. HTTP readiness does not mean indexing has finished: inspect
`GET /v1/index`. Symbol queries return `503 index_not_ready` until an index is
available, while health and job-status requests remain responsive.

## Background operation

```sh
facts-tool serve --server-config /workspace/server.yaml --daemon
```

The parent exits successfully only after the listener, watcher and saved settings
are ready. Without a configured log file, daemon logs go to
`/workspace/server.yaml.log`; `--log-file` or `logging.file` selects another path.
The PID and instance lock use `/workspace/server.yaml.pid`.
The same configuration cannot start a second server
while its lock is held. The PID file is left empty after shutdown.

Stop with `POST /v1/shutdown`, `SIGTERM` or `SIGINT`. Shutdown stops watching,
cancels queued jobs, waits for running native analysis, and reaps compatibility
worker processes. The daemon flag is
not persisted; omit it on a later invocation to run in the foreground.

## Options

| Option | Meaning |
|---|---|
| `--server-config FILE` | Read and update server settings; default `.facts-tool-server.yaml` in the invocation directory |
| `--host IP` | Numeric IPv4 or IPv6 address; initially `127.0.0.1` |
| `--port N` | Listener port; initially `0` for automatic allocation |
| `--daemon` | Run in the background and wait for startup readiness |
| `--log-file FILE` | Append structured server logs to this file in foreground or daemon mode |
| `--log-level LEVEL` | Server logging: `off`, `error`, `warning`, `info`, `debug`, `trace`; default `info` |
| `-v N`, `--verbose N` | Server verbosity: `0` error, `1` info, `2` debug, `3` trace |
| `--working-directory DIR` | Server configuration discovery and compatibility-job working directory |
| `--conf FILE`, `-c FILE` | Server-owned project database |
| `--config FILE` | Server-side project defaults and facts-template YAML |
| `--watch` | Enable recursive monitoring of registered repositories |
| `--no-watch` | Disable monitoring while keeping its exclusion settings |
| `--debounce-ms N` | Watch debounce, default `500`; range `1`–`3600000` |
| `--timeout N` | Compatibility subprocess deadline, default `3600`; range `1`–`86400` seconds |
| `--import-arg=VALUE` | Repeated argument tokens for automatic reimport |
| `--extract-arg=VALUE` | Repeated argument tokens for automatic extraction |
| `--token TOKEN` | Bearer token; alternatively set `FACTS_TOOL_API_TOKEN` |

Explicit server options override saved values. `--watch` and `--no-watch` persist
the monitoring enablement flag. Configuration paths are normalized to absolute
paths. Only deprecated command jobs can override the server's database defaults.
Watch roots come from the project database: the active clone of every registered
repository, subject to exclusions. There is no separate directory list to maintain.
Monitoring defaults to enabled on Linux and disabled on other platforms.
An explicit `--watch` on a platform without inotify fails at startup.

## Saved configuration

```yaml
schema_version: 1
host: 127.0.0.1
port: 42817
working_directory: /workspace/project
logging:
  file: /workspace/logs/facts-tool.jsonl
  level: info
watch:
  enabled: true
  exclude_repositories: []
  exclude_clones: []
  exclude_directories: []
  exclude_patterns: []
defaults:
  - --conf
  - /workspace/project.db
  - --config
  - /workspace/defaults.yaml
import_arguments: []
extract_arguments: []
debounce_ms: 500
timeout_seconds: 3600
```

The port above is illustrative. Settings are atomically rewritten after successful
startup. Changes to this file take effect on restart. Tokens are never saved.
Logging paths and levels are independent of the server configuration filename.
See [Logging and verbosity](09-logging.md) for relative paths, precedence,
structured events and the distinction between server and command verbosity.
The former `watch_directories` setting is ignored and removed when settings are
saved; register repositories in the project database instead. Repository and
active-clone changes are detected while the server runs.

All endpoints require `Authorization: Bearer TOKEN` when a token is configured,
including health checks. Binding to a non-loopback address requires a token.
Use `FACTS_TOOL_API_TOKEN` to keep it out of command-line history. HTTP has no
built-in TLS or browser CORS support; the API is intended for trusted CLI and agent
clients with the server account's filesystem permissions.

Continue with [requests and jobs](02-requests-and-jobs.md) or
[automatic reimport and indexing](03-watching-directories.md).
