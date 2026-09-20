# Deployment and operation

← [User guide index](../README.md) · [Table of contents](../toc.md)

Install the native binary using [REST installation](05-installation.md). Run it
under the account that owns the source checkout and writable project/facts
databases. Server jobs use that account's filesystem access, compiler environment
and configured working directory. The compilation database's source paths,
headers and toolchain must exist on the server host.

## Start in the foreground

The examples use an existing checkout at `/workspace/project` and an existing
project database at `/workspace/project.db`. Replace them with absolute paths
for your machine. Initialize a new project using the
[quick start](../01-introduction/04-quick-start.md) first.

```bash
mkdir -p "$HOME/.config/facts-tool"
export FACTS_TOOL_API_TOKEN="$(python3 -c 'import secrets; print(secrets.token_urlsafe(32))')"
"$HOME/.local/bin/facts-tool" serve \
  --server-config "$HOME/.config/facts-tool/server.yaml" \
  --working-directory /workspace/project \
  --conf /workspace/project.db \
  --host 127.0.0.1 --port 0
```

Keep the token available to your client. `--conf` is optional when ordinary
configuration discovery resolves the intended database. If using a CLI defaults
YAML, also pass `--config /absolute/path/defaults.yaml`; it is separate from
`--server-config`.

Port `0` allocates an available port and saves the actual number in `server.yaml`.
The server prints `facts-tool: server ready at HOST:PORT`. On later starts, omit
`--port` to reuse the saved port; if it is occupied, the server allocates and saves
another. Supply `--port 42817` on every start when clients require that fixed port;
an occupied explicitly requested port fails startup.

After reading the real port, check readiness from another terminal with the same
token:

```bash
API=http://127.0.0.1:42817
curl --fail --silent --show-error \
  -H "Authorization: Bearer $FACTS_TOOL_API_TOKEN" "$API/health"
```

All endpoints, including health checks, require the token when configured.
Tokens are not written into the server YAML. Foreground logs default to stderr
at `info` level. Set `logging.file` in the server YAML or pass `--log-file FILE`
to write to a file; `--log-level debug` or `-v 2` increases server detail.
These settings are saved independently of the configuration filename. See
[Logging and verbosity](09-logging.md) for configuration and structured events.
Press Control-C for graceful shutdown and draining of queued log records.

## Run as a daemon

After the initial configuration has been saved, use the same token and settings:

```bash
"$HOME/.local/bin/facts-tool" serve \
  --server-config "$HOME/.config/facts-tool/server.yaml" --daemon
tail -f "$HOME/.config/facts-tool/server.yaml.log"
```

The example uses the default daemon log destination. To choose an independent
path and level, start with these options; the same destination also works when
running in the foreground:

```bash
mkdir -p "$HOME/.local/state/facts-tool"
"$HOME/.local/bin/facts-tool" serve \
  --server-config "$HOME/.config/facts-tool/server.yaml" --daemon \
  --log-file "$HOME/.local/state/facts-tool/server.jsonl" --log-level info
tail -f "$HOME/.local/state/facts-tool/server.jsonl"
```

The parent prints the listener address after successful startup, including when
logging is off. An unusable log destination fails startup without reporting
readiness. The adjacent `server.yaml.pid` file holds
the instance PID and lock. A second server cannot use that same configuration
while the lock is held. Stop it through the API:

```bash
curl --fail --silent --show-error -X POST \
  -H "Authorization: Bearer $FACTS_TOOL_API_TOKEN" "$API/v1/shutdown"
```

`SIGTERM` and `SIGINT` also trigger graceful shutdown. Restart by waiting for
shutdown to finish and running `serve ... --daemon` again. Daemon mode alone
does not arrange recovery after process failure or startup after a host reboot;
use an operating-system service manager for ongoing supervision.

## Supervise with systemd on Linux

Use a user service to keep the server under the account that owns the project.
Create the configuration directories and a token file:

```bash
mkdir -p "$HOME/.config/systemd/user" "$HOME/.config/facts-tool"
(umask 077; python3 -c 'import secrets; print("FACTS_TOOL_API_TOKEN=" + secrets.token_urlsafe(32))' \
  > "$HOME/.config/facts-tool/server.env")
chmod 600 "$HOME/.config/facts-tool/server.env"
```

Share the token with authorized clients; rotating it requires restarting the
server and updating those clients. Save this unit as
`~/.config/systemd/user/facts-tool.service`, replacing the project paths:

```ini
[Unit]
Description=facts-tool REST server

[Service]
Type=simple
WorkingDirectory=/workspace/project
EnvironmentFile=%h/.config/facts-tool/server.env
ExecStart=%h/.local/bin/facts-tool serve --server-config %h/.config/facts-tool/server.yaml --working-directory /workspace/project --conf /workspace/project.db
Restart=on-failure
RestartSec=3
TimeoutStopSec=30

[Install]
WantedBy=default.target
```

The service runs the foreground process, so omit `--daemon`. Without a configured
`logging.file`, structured events go to stderr and systemd collects them in the
journal. If the saved YAML specifies a log file, events go there; remove that key
to use the journal. Startup readiness still goes to stdout.
Systemd owns the process lifecycle. `Type=simple` reports the process started;
verify HTTP readiness separately; see the official
[systemd service reference](https://github.com/systemd/systemd/blob/main/man/systemd.service.xml).
The unit reuses saved host, port and watch
settings. Add `--port 42817` to `ExecStart` for a fixed port. If compilation needs
additional environment variables, configure them for the service; it does not
read your interactive shell profile.

```bash
systemctl --user daemon-reload
systemctl --user enable --now facts-tool.service
systemctl --user status facts-tool.service
journalctl --user -u facts-tool.service -f
```

Use `systemctl --user stop facts-tool.service` to stop, and
`systemctl --user restart facts-tool.service` after a configuration or token
change, including changes to `logging.level` or `logging.file`. After editing the
unit itself, run `daemon-reload` before restarting.
If the service must start at boot and continue after logout, enable user lingering
with `loginctl enable-linger "$USER"`; local policy may require an administrator.

## Monitor and refresh registered repositories

On Linux, monitoring is enabled by default. The server discovers all repositories
in its project database and monitors each repository's active clone. Register
repositories through `repo add` or `import`; switch an active clone with
`repo switch`. The running watcher follows these database changes automatically.
There is no separate list of source roots in the service command or server YAML.

Use `--no-watch` to disable monitoring and `--watch` to enable it again. These
flags persist the setting. Repository, clone, directory and pattern exclusions
belong in the server YAML's `watch` section; each clone's `.gitignore` rules apply
by default. Restart the service after changing YAML exclusions.

If a compilation database lives outside the monitored clones or in an ignored
build directory, supply explicit import arguments. The server watches that
`compile_commands.json` as a control input; source exclusions still apply:

```text
--import-arg=-p --import-arg=/workspace/project-build
--extract-arg=-o --extract-arg=/workspace/facts.db
```

Perform the initial import and extraction before relying on watching; startup
itself does not index. Inotify events trigger debounced reimport and forced
reindexing of included sources. See
[Repository monitoring](03-watching-directories.md) for exclusions, Git ignore
rules, cache invalidation and failure reporting. Inspect `/v1/watch` or
`Client.watch_status()` and the reported job IDs when a refresh fails.

## Network access and upgrades

The default listener is loopback. Binding to a non-loopback address requires
a bearer token. The server provides HTTP without built-in TLS and rejects
browser-origin requests; it has no browser CORS interface. Use it from trusted
Python or command-line clients. For remote clients, keep the backend private
and use an authenticated tunnel or a TLS-terminating proxy configured for these
clients. Do not send bearer tokens over an untrusted plaintext network.

Before upgrading, let required jobs finish and stop the daemon or service.
Rebuild and reinstall the `facts-tool` component, then restart and check health.
Saved server settings and databases persist; queued jobs and retained job/output
records do not survive restart. Shutdown cancels queued and active jobs. Read
the saved port again if startup had to allocate a replacement.
