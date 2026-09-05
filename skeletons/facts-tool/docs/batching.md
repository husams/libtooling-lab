# Batch extraction

`facts-tool-batch` runs one `facts-tool` process per source with bounded
parallelism. It requires Python 3.10 or newer and finds the `facts-tool`
executable through `PATH`.

Install the wrapper and every `facts_batch_*.py` helper together in one
directory on `PATH`.

```sh
install -d ~/.local/bin
install scripts/facts-tool-batch scripts/facts_batch_*.py ~/.local/bin/
```

Import a compilation database once, then run either mode:

```sh
facts-tool import -p build -c project.db
facts-tool-batch extract -j 4 -o batch-out -c project.db -p build
facts-tool-batch dependency -j 4 -o batch-out -c project.db src/a.cpp src/b.cpp
```

Use `--files-from LIST` for one source path per line; `--files-from -` reads
standard input. `-p` only enumerates the `file` entries in a directory or JSON
compilation database; it does not import them. Explicit and list paths are
resolved relative to the current directory. Compilation database paths are
resolved relative to each entry's `directory`, with a relative directory
anchored at the database's parent.

Each canonical source receives a database named
`<basename>-<sha256-of-canonical-path>.db` in the output directory. Logs use the
same stem and add `-extract.log` or `-dependency.log`, so the two modes can be
run sequentially against the same output directory. Databases are independent
per source and are not copied or merged. The output directory is exclusively
locked for the duration of one invocation.

`-j` defaults to the CPU count (or one). Higher values can increase RAM use
substantially because every child has its own compiler and SQLite workload.
Failed jobs continue independently; the final summary names every failed log
and the command exits nonzero. SIGINT and SIGTERM stop active children, reap
them, and prevent further launches.

Dependency jobs that fail with SQLite `database is locked` or `database table is
locked` are retried once, serially, after the first pass. Other failures are
reported in the final summary and are never retried.

The wrapper forwards `--conf`, `--config`, repeated `--extra-arg`, and
`--verbose` unchanged, and places `--` before every source path. Paths with
spaces are passed as argv values without shell evaluation.
