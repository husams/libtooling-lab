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

Run the complete Python API BDD suite without selection:

```console
uv run pytest tests/bdd
```

Run the physical-file gate and every example:

```console
uv run python scripts/check_file_sizes.py
uv run python scripts/check_import_boundaries.py
uv run pytest tests/test_examples.py
```

Build a wheel, install it with pip outside the checkout, and execute every API
BDD scenario against that installed artifact:

```console
uv run python scripts/run_installed_bdd.py
```

`tests/test_distributions.py` builds wheel and sdist, installs each with pip in
a separate temporary Python environment outside the checkout, and runs a
paired-database query without `PYTHONPATH` or editable installation.

The native facts-tool regression remains owned by the repository root:

```console
cd ..
./scripts/run-e2e.sh build
```

Record the unfiltered scenario/test counts, Python version, OS, architecture,
and exact commit for review. Supported release evidence covers CPython 3.12 and
3.13 on macOS and Linux/RHEL-compatible systems; the wheel is platform-neutral
and the runtime has no native dependency.

Generated `uv.lock`, wheel/sdist contents, `.venv`, caches, and build outputs
are excluded from the 100-line source gate. Every hand-authored source, test,
step, fixture-code, example, validation script, config, and documentation file
under `python/` is checked.
