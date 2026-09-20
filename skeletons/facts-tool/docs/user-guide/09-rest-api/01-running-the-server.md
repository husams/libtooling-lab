# Running facts-tool as a server

`facts-tool` remains a CLI. The additional `serve` command exposes its commands
over HTTP for applications and agents. Existing CLI invocations are unchanged.

```sh
facts-tool serve --server-config /workspace/server.yaml \
  --host 127.0.0.1 --port 0 \
  --conf /workspace/project.db --config /workspace/defaults.yaml
```

`--config` is the usual CLI defaults YAML. `--server-config` is a separate file
for listener, watcher and worker settings. Both project options are optional:
ordinary CLI configuration discovery still applies to jobs.

Port `0` asks the operating system to allocate an available port. After binding,
the server writes its actual host and port to the server configuration and prints
`facts-tool: server ready at HOST:PORT`. Read that port before making requests.
On restart, the saved port is reused when available; if it has been taken and
no explicit `--port` was supplied, a new available port is saved automatically.
An occupied port supplied explicitly is a startup error.

## Background operation

```sh
facts-tool serve --server-config /workspace/server.yaml --daemon
```

The parent exits successfully only after the listener, watcher and saved settings
are ready. Logs go to `/workspace/server.yaml.log`; the PID and instance lock use
`/workspace/server.yaml.pid`. The same configuration cannot start a second server
while its lock is held. The PID file is left empty after shutdown.

Stop with `POST /v1/shutdown`, `SIGTERM` or `SIGINT`. Shutdown stops watching,
cancels queued and active jobs, and reaps worker processes. The daemon flag is
not persisted; omit it on a later invocation to run in the foreground.

## Options

| Option | Meaning |
|---|---|
| `--server-config FILE` | Read and update server settings; default `.facts-tool-server.yaml` in the invocation directory |
| `--host IP` | Numeric IPv4 or IPv6 address; initially `127.0.0.1` |
| `--port N` | Listener port; initially `0` for automatic allocation |
| `--daemon` | Run in the background and wait for startup readiness |
| `--working-directory DIR` | Working directory for all CLI jobs |
| `--conf FILE`, `-c FILE` | Default project database for jobs |
| `--config FILE` | Default CLI YAML for jobs |
| `--watch DIR` | Recursive directory watch; repeat for multiple roots |
| `--no-watch` | Clear all saved watch directories |
| `--debounce-ms N` | Watch debounce, default `500`; range `1`–`3600000` |
| `--timeout N` | Per-job deadline in seconds, default `3600`; range `1`–`86400` |
| `--import-arg=VALUE` | Repeated argument tokens for automatic reimport |
| `--extract-arg=VALUE` | Repeated argument tokens for automatic extraction |
| `--token TOKEN` | Bearer token; alternatively set `FACTS_TOOL_API_TOKEN` |

Explicit server options override saved values. An explicit `--watch` list replaces
the saved list. Job arguments override the server's default `--conf`/`--config`.
Configuration paths and watch roots are normalized to absolute paths.

## Saved configuration

```yaml
schema_version: 1
host: 127.0.0.1
port: 42817
working_directory: /workspace/project
watch_directories:
  - /workspace/project
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

All endpoints require `Authorization: Bearer TOKEN` when a token is configured,
including health checks. Binding to a non-loopback address requires a token.
Use `FACTS_TOOL_API_TOKEN` to keep it out of command-line history. HTTP has no
built-in TLS or browser CORS support; the API is intended for trusted CLI and agent
clients with the server account's filesystem permissions.

Continue with [requests and jobs](02-requests-and-jobs.md) or
[automatic reimport and indexing](03-watching-directories.md).
