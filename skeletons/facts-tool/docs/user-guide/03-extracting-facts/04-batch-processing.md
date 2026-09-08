# Batch Processing

`facts-tool-batch` runs one native `facts-tool` process per source file with
bounded parallelism. It is a process-fanout wrapper around the same
`extract`/`analyse dependency` commands described elsewhere in this guide -
not a different extraction engine, and not a replacement for `import`.

```console
$ facts-tool-batch --help
usage: facts-tool-batch [-h] {extract,dependency} ...
Run facts-tool once per source with bounded concurrency.
```

## Installation

The wrapper is a plain Python 3.10+ argparse script, not a compiled binary.
Install it together with every `facts_batch_*.py` helper module in one
directory on `PATH`:

```sh
install -d ~/.local/bin
install scripts/facts-tool-batch scripts/facts_batch_*.py ~/.local/bin/
```

## Helper modules

The wrapper's logic is split across six small helper modules under
`scripts/`:

| Module | Responsibility |
|---|---|
| `facts_batch_args.py` | argument parsing shared by both subcommands |
| `facts_batch_sources.py` | resolving the source list (positional args, `--files-from`, `-p` enumeration) |
| `facts_batch_lock.py` | exclusive-locking the output directory for the invocation |
| `facts_batch_retry.py` | the one-pass serial retry for lock-contention failures |
| `facts_batch_process.py` | spawning and supervising each child `facts-tool` process |
| `facts_batch_run.py` | the top-level `extract`/`dependency` command entry points |

## Running a batch

Import a compilation database once (batch mode never imports - see
[importing compile commands](../02-projects-and-configuration/02-importing-compile-commands.md)),
then run either mode:

```sh
facts-tool import -p build -c project.db
facts-tool-batch extract -j 4 -o batch-out -c project.db -p build
facts-tool-batch dependency -j 4 -o batch-out -c project.db src/a.cpp src/b.cpp
```

`facts-tool-batch extract --help`:

```text
usage: facts-tool-batch extract [-h] [-j JOBS] -o OUTPUT_DIR [-c CONF]
                                [--config CONFIG] [--extra-arg EXTRA_ARG]
                                [-v {0,1,2,3}] [--files-from FILES_FROM]
                                [-p COMPILATION_DATABASE] [sources ...]
```

`facts-tool-batch dependency` accepts the identical option set. Both
subcommands forward `--conf`, `--config`, repeated `--extra-arg`, and
`--verbose` unchanged to each child `facts-tool` invocation, and place `--`
before every source path so paths containing spaces are passed as literal
argv values without shell evaluation.

### Options

| Flag | Meaning |
|---|---|
| `-j`, `--jobs JOBS` | maximum simultaneous `facts-tool` processes (default: CPU count, or 1) |
| `-o`, `--output-dir OUTPUT_DIR` | directory for per-source databases and logs (required) |
| `--files-from FILES_FROM` | source list, one path per line; `-` reads stdin |
| `-p`, `--compilation-database` | enumerate sources from a `compile_commands.json` - **does not import them** |
| `-c`, `--conf` / `--config` / `--extra-arg` / `-v` | forwarded unchanged to each child process |

`-p` only enumerates `file` entries from a directory or JSON compilation
database; it never imports them into `-c`'s project database. If the
sources were never imported, run `import` first - batch mode assumes the
project database already knows the compile commands it is enumerating.
Explicit and `--files-from` paths resolve relative to the current
directory; compilation-database paths resolve relative to each entry's
`directory` (a relative `directory` anchors at the database's parent).

## Output layout

Each canonical source gets its **own independent database**, named
`<basename>-<sha256-of-canonical-path>.db`, plus a same-stemmed log file
(`-extract.log` or `-dependency.log`, so both modes can run sequentially
against the same output directory without colliding). Verified real output
for a 2-source run:

```console
$ facts-tool-batch extract -j 2 -o batch-out -c demo.db -p proj
facts-tool-batch: 2 succeeded, 0 failed
```

```text
batch-out/
  .facts-tool-batch.lock
  main.cpp-392451ae79cb...9cacbcf-extract.log
  main.cpp-392451ae79cb...9cacbcf.db
  shapes.cpp-8cc898d837b2...ecfe7d1b-extract.log
  shapes.cpp-8cc898d837b2...ecfe7d1b.db
```

The hashes are the SHA-256 of each source's canonical absolute path, so they
differ between checkouts. `.facts-tool-batch.lock` is the invocation lock file
described below.

Databases are independent per source and are never copied or merged by the
wrapper - combining per-source facts across sources is the caller's
responsibility, whether by hand or by opening multiple paired databases
through the Python SDK.

## Locking and retry behavior

- The output directory is **exclusively locked** for the duration of one
  invocation, so two batch runs cannot write into the same directory
  concurrently.
- `-j` defaults to the CPU count (or 1 if that is unavailable), so
  `--help` prints the number of cores on the machine you run it on rather
  than a fixed value. Higher values can substantially increase RAM use,
  since every child process runs its own compiler front end and SQLite
  workload.
- Failed jobs never block the rest of the batch - each source is
  independent. The final summary names every failed log, and the command
  exits nonzero if any job failed.
- In `dependency` mode specifically, a job that fails with SQLite's
  `database is locked` or `database table is locked` is retried **once,
  serially**, after the first parallel pass completes. Every other kind of
  failure is reported in the final summary and is never retried.
- SIGINT and SIGTERM stop every active child, reap them, and prevent
  further launches - a batch run can be interrupted cleanly.

## Requirements

`facts-tool-batch` requires Python 3.10 or newer and finds the `facts-tool`
executable via `PATH`. It is not a compiled binary, so it must be run with a
Python interpreter that satisfies that version requirement, separate from
whichever Python you use for the [Python SDK](../05-python-sdk/01-getting-started.md).
