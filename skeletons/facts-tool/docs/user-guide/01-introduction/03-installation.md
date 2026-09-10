# Installation

`facts-tool` has two parts you install separately: the native C++ tool
(`facts-tool` itself, plus the `facts-tool-batch` wrapper script) and the
Python query SDK (`facts-tool-query`, import name `facts_tool`). You only
need the SDK if you plan to query the databases from Python or from an
agent; the native tool alone is enough to extract facts and run call
graphs.

## Building the native tool

`facts-tool` is built with CMake and Ninja against Homebrew LLVM. Its own
`CMakeLists.txt` documents the exact macOS invocation in a header comment:

```bash
cmake -G Ninja -B build \
  -DCMAKE_PREFIX_PATH=$(brew --prefix llvm) \
  -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++ .
cmake --build build
```

Run these two commands from the `facts-tool` project root. A few points
worth knowing before you do:

- **Never build with Apple's system clang.** The `-DCMAKE_CXX_COMPILER`
  flag must point at the Homebrew LLVM `clang++`, because Apple's clang
  does not ship the LibTooling headers this project needs, and mixing the
  two toolchains' ABIs will fail in confusing ways.
- The project fetches its own dependencies via CMake's `FetchContent` the
  first time you configure: CLI11, itlib, FTXUI, `yaml-cpp` 0.9.0, libgit2
  1.9.7, and the SQLite amalgamation. The first configure therefore needs
  network access. libgit2 is built with its network/HTTPS backends off and
  its bundled zlib/regex/hash implementations on, so it needs nothing beyond
  what the fetch itself brings in - it is used only to read the project's
  own `.git` remotes, for the `{project_name}` template placeholder.
- SQLite is vendored as a static library rather than relying on whatever
  version the system provides - the storage layer needs a SQLite new enough
  to support `RETURNING` (3.35+), which older system SQLite installs
  (including RHEL 9's stock 3.34) don't have. Pass
  `-DFACTS_SYSTEM_SQLITE=ON` to link the system library instead; CMake then
  requires 3.35 or newer.
- The build also needs C enabled at the `project()` level, because some of
  LLVM's own CMake modules (`FindLibEdit`) run C header checks even though
  the tool itself is pure C++.

Once the build finishes, the binary is at `build/facts-tool` relative to
wherever you ran `cmake -B build` from. Always invoke that build output
directly (or put it on your `PATH` yourself) rather than relying on a
possibly-stale copy that might already be installed somewhere like
`~/.local/bin/facts-tool` - a stale binary from an older build won't match
the schema or CLI surface documented here.

### Verifying the native build

`facts-tool` does not have a `--version` flag; running it with no
subcommand exits with a usage error (exit code 2) rather than printing
version information. The practical way to confirm you're running a working,
correctly configured build is to run a real command against it, for
example:

```console
$ ./build/facts-tool config show
```

If that prints a resolved configuration (see
[02-projects-and-configuration/04-config-command](../02-projects-and-configuration/04-config-command.md)
for what the output means), the binary is built and runnable. To confirm
*which* build a given binary corresponds to, compare its file's modified
time against your last `cmake --build build`, or simply always invoke the
binary by its full build path so there's no ambiguity about which copy you
are running.

### Linux (RHEL 9-family) builds

`facts-tool` also supports RHEL 9 / AlmaLinux 9 / Rocky 9 via a dedicated
helper script, `scripts/build-rhel9.sh`, run from the project root:

```bash
./scripts/build-rhel9.sh              # install dependencies, build, run ctest
DEPS_ONLY=1 ./scripts/build-rhel9.sh  # install dependencies only, no build
SKIP_TESTS=1 ./scripts/build-rhel9.sh # build only, no pytest venv, no ctest
```

This produces `build-rhel9/facts-tool` under the project root. The extra
plumbing exists because RHEL 9 differs from the macOS lab environment in
three ways the script accounts for:

- **Compiler.** The tool uses C++23 features (`std::expected`,
  `std::ranges::to`) that need GCC 14 or newer; RHEL 9's system GCC is 11.
  The script installs `gcc-toolset-15` (configurable via `GCC_TOOLSET`,
  minimum 14) to supply a compatible compiler and libstdc++ headers.
- **SQLite.** Stock RHEL 9's `sqlite-devel` is 3.34.1 and ships no static
  library, so the script fetches the official SQLite amalgamation and
  builds it once, caching the sources under `.deps/` for later runs.
- **Clang/LLVM.** `clang-devel` and `llvm-devel` supply the
  `ClangConfig.cmake`/`LLVMConfig.cmake` files CMake's `find_package(Clang)`
  needs, and stay dynamically linked - running the resulting binary on
  another host requires that host to have `clang-libs`/`llvm-libs`
  installed too.

Environment variables documented in the script's own header let you skip
dependency installation (`SKIP_DEPS=1`), point at a different build
directory (`BUILD_DIR`), or control job parallelism (`JOBS`); see the
script itself for the full list.

## Installing the Python SDK

The Python SDK (`facts-tool-query`, `requires-python = ">=3.12"`, zero
runtime dependencies) lives under `python/` in the `facts-tool` project and
is built with `hatchling`. Two supported ways to get it onto a machine:

### Option 1: `uv` dev environment (for working inside the checkout)

If you're developing against the checkout itself, `python/` already has a
`uv`-managed environment with a lockfile:

```bash
cd python
uv sync --locked
```

This produces `python/.venv`, with `facts_tool` installed in editable mode
against `python/src/facts_tool`. Note that this environment is `uv`-managed
and does **not** include a plain `pip` - use `uv pip` or `uv run` inside it
if you need package-management commands beyond what `uv sync` already did.

### Option 2: build a wheel and install it elsewhere

To install the SDK into a separate environment (for example, to test it the
way an end user would, isolated from the checkout), build a wheel first and
then install that wheel:

```bash
cd python
uv build --out-dir /path/to/dist

uv venv --seed --python 3.12 /path/to/venv
/path/to/venv/bin/python -m pip install /path/to/dist/facts_tool_query-0.2.0-py3-none-any.whl
```

This is the pattern to use for CI-style "installed, not editable" testing;
the project's own `python/scripts/run_installed_bdd.py` automates a fuller
version of exactly this round trip (build wheel, fresh venv, pinned
`pytest`/`pytest-bdd`, run the BDD suite against the installed package with
`PYTHONPATH`/`PYTHONHOME` stripped).

### Verifying the installed SDK

The package name (`facts-tool-query`) and the import name (`facts_tool`)
are different - `pip`/`uv` operate on the former, `import` statements use
the latter. Both an editable checkout install and a wheel install report
the same version string, so if you need to tell them apart, check the
module's `__file__` instead:

```console
$ python -c "
from importlib.metadata import version, distribution
import facts_tool
print(version('facts-tool-query'))
print(facts_tool.__file__)
"
0.2.0
/path/to/facts-tool/python/src/facts_tool/__init__.py   # editable checkout install
```

An installed wheel reports the same version but a `__file__` under a
`site-packages/facts_tool/...` path instead of the checkout's `src/`
directory. You can also check
`'direct_url.json' in [f.name for f in distribution('facts-tool-query').files]`
- `True` for an editable install, absent for a wheel install.

For a smoke test that actually exercises a paired database rather than
just checking the version string, the package ships
`python/scripts/installed_smoke.py`, which opens a facts/project pair and
asserts a known query returns the expected result:

```console
$ python python/scripts/installed_smoke.py <facts.db> <project.db>
{"package": "facts-tool-query", "query": "pass"}
```

Passing a third argument (`graph`) additionally asserts that the paired
database's latest persisted call-graph run has `status == "complete"`.

### Supported platforms

The Python SDK's runtime has zero dependencies and supports Python 3.12 and
3.13 on macOS and Linux, including RHEL-compatible distributions.

## What's next

With both parts installed, move on to
[04-quick-start](04-quick-start.md) for a hands-on ten-minute tour: build a
tiny project, import it, extract facts, and query them from both the CLI
and Python.
