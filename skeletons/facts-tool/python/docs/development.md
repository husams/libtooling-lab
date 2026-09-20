# UV development and validation

UV owns the environment and committed lock:

```console
uv sync --locked
uv run ruff format --check src tests examples scripts
uv run ruff check src tests examples scripts
uv run mypy --strict src
uv run pytest
uv build
```

Run the complete Python API BDD suite without selection, including REST scenarios
against a real native server. Set the native executable and target compiler first:

```console
export FACTS_TOOL_NATIVE=/absolute/path/to/build/facts-tool
export FACTS_CLANGXX=/absolute/path/to/clang++
uv run pytest tests/bdd
```

Run the physical-file gate and every example:

```console
uv run python scripts/check_file_sizes.py
uv run python scripts/check_import_boundaries.py
uv run pytest tests/test_examples.py
```

Build a wheel, install it with pip outside the checkout, and execute every API
BDD scenario against that installed artifact, including the optional REST extra:

```console
uv run python scripts/run_installed_bdd.py
```

The installed BDD runner requires both executable paths above and clears inherited
pytest selection options. REST scenarios launch real HTTP servers and CLI workers;
they do not mock responses or replace the native executable. Ordinary pytest runs
can skip native scenarios if the native tool is unavailable; the installed gate
requires it so a missing tool cannot silently remove REST coverage.

`tests/test_distributions.py` builds wheel and sdist, installs each with pip in
a separate temporary Python environment outside the checkout, and runs a
paired-database query without `PYTHONPATH` or editable installation.

The native facts-tool regression remains owned by the repository root:

```console
cd ..
bash scripts/run-e2e.sh build
```

Record the unfiltered scenario/test counts, Python version, OS, architecture,
and exact commit for review. Supported release evidence covers CPython 3.12 and
3.13 on macOS and Linux/RHEL-compatible systems; the wheel is platform-neutral
and the local SQLite query runtime has no native dependency. REST clients require
HTTPX and communicate with the separately installed native server.

Generated `uv.lock`, wheel/sdist contents, `.venv`, caches, and build outputs
are excluded from the 100-line source gate. Every hand-authored source, test,
step, fixture-code, example, validation script, config, and documentation file
under `python/` is checked.
