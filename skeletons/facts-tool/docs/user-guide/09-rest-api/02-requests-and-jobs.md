# REST requests and asynchronous jobs

← [User guide index](../README.md) · [Table of contents](../toc.md)

Use the host and allocated port saved by `serve`. Examples below assume:

```sh
API=http://127.0.0.1:42817
```

Add `-H "Authorization: Bearer $FACTS_TOOL_API_TOKEN"` to requests when configured.

## Endpoints

| Method and path | Response |
|---|---|
| `GET /health` | `200`, `{"status":"ok"}` |
| `GET /openapi.json` | `200`, OpenAPI 3.1 schema for the live commands, requests, jobs and authentication |
| `GET /v1/commands` | `200`, `{"commands":[{"path":"import","endpoint":"/v1/commands/import"}, ...]}` |
| `POST /v1/commands/{path}` | `202`, asynchronous job; body contains command arguments |
| `POST /v1/jobs` | `202`, asynchronous job; body contains the complete CLI argument vector |
| `GET /v1/jobs` | `200`, `{"jobs":[...]}` metadata without stdout/stderr |
| `GET /v1/jobs/{id}` | `200`, job state, exit code and captured output |
| `DELETE /v1/jobs/{id}` | `200`, requests cancellation and returns current job state |
| `GET /v1/watch` | `200`, watcher activity and latest job IDs |
| `POST /v1/shutdown` | `202`, `{"status":"stopping"}` |

Command paths mirror CLI nesting: `analyse/call-graph`, `symbol/index/clear`,
`repo/add-clone`, and so on. Discovery comes from the live CLI parser, including
aliases. Agents can read `/openapi.json` for the machine-readable interface.
Groups accept their child name in the argument list. `serve` itself is
excluded from jobs; shutdown uses its dedicated endpoint.

## Submit commands

```sh
curl -sS "$API/v1/commands"
curl -sS -X POST "$API/v1/commands/import" \
  -H 'Content-Type: application/json' \
  -d '{"arguments":["-p","/workspace/project"]}'
```

The response includes a string `id`; its `Location` header is `/v1/jobs/{id}`.
Wait for a successful import before submitting dependent work:

```sh
curl -sS "$API/v1/jobs/1"
curl -sS -X POST "$API/v1/commands/extract" \
  -H 'Content-Type: application/json' \
  -d '{"arguments":["-o","/workspace/facts.db"]}'
```

The generic endpoint offers the same command surface:

```sh
curl -sS -X POST "$API/v1/jobs" \
  -H 'Content-Type: application/json' \
  -d '{"arguments":["symbol","list","-f","/workspace/facts.db"]}'
```

Pass each CLI token as one JSON string. Do not include the `facts-tool` executable
or shell quotation around a token. Arguments are passed directly to the process;
shell expansion, pipelines and command substitution are not evaluated. Use
`["--help"]` on any command endpoint for that command's current options.

All CLI commands, options and exit codes are available. The terminal-only
`symbol/browser` command returns the noninteractive `symbol list` view over HTTP.
Commands with `--format json` still write JSON to the job's `stdout` string;
clients may parse that string after the job succeeds.

## Job lifecycle and limits

Jobs move from `queued` to `running`, then `succeeded`, `failed` or `cancelled`.
`exit_code` starts as `null`. Completed jobs include `stdout`, `stderr`,
`truncated`, `timed_out` and their exit code. Output is published on completion,
not streamed while the command runs. Timestamps use Unix epoch milliseconds:
`created_at`, then `started_at` and `finished_at` as those events occur.

An HTTP `202` means accepted, not completed successfully. A valid request whose
CLI command rejects an option produces a failed job with the usual CLI exit code.
Clients should poll the individual job and inspect its final state and stderr.
Cancellation sends `SIGTERM` to the worker process group, then forces termination
after a grace period. A timeout produces `failed` with `timed_out: true`.

The server executes one worker at a time, keeping database writes from API jobs
and watcher jobs serialized while HTTP remains responsive. It accepts up to 64
waiting jobs and retains up to 128 job records, evicting old completed records.
Each output stream is capped at 4 MiB; `truncated: true` reports discarded output.
For larger results use the command's database output and the Python SDK.
Job records are in memory and disappear at restart; persisted facts remain.

Malformed JSON, invalid argument types or unknown command paths return `400`.
Unknown endpoints or missing/evicted jobs return `404`; wrong methods return
`405`; queue saturation returns `429`. Authentication failures return `401`,
and browser-origin requests return `403`. Bodies are limited to 1 MiB, headers
to 16 KiB, and each HTTP request has a 30-second I/O deadline.

## Python clients

Use the SDK's optional [REST client](../05-python-sdk/11-rest-client.md) for
synchronous `Client` or asynchronous `AsyncClient` access to every endpoint.
Both expose typed job results, polling, cancellation and distinct HTTP/job errors.
