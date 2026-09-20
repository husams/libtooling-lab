# Installing the REST server and Python client

← [User guide index](../README.md) · [Table of contents](../toc.md)

The REST server is part of the native `facts-tool` binary. The Python REST
wrapper is an optional extra of `facts-tool-query`. Install each on the machine
that needs it; a remote Python client needs neither Clang nor the project databases.

| Machine or task | Install |
|---|---|
| Native CLI or REST server | Build and install `facts-tool`, including its runtime libraries |
| Python application or agent using HTTP | Python 3.12+ and `facts-tool-query[rest]` from this checkout |
| Python querying SQLite files directly | Base `facts-tool-query`, no REST dependency |
| Automatic directory monitoring | Run the native server on Linux with inotify |

## Build the native server

Use a checkout containing the `serve` command and the REST client additions.
Run native build commands from `skeletons/facts-tool` inside the repository.
The [main installation chapter](../01-introduction/03-installation.md) explains
dependency versions, caches and compiler requirements.

### RHEL 9, AlmaLinux 9 and Rocky 9

```bash
cd skeletons/facts-tool
./scripts/build-rhel9.sh
./build-rhel9/facts-tool serve --help
cmake --install build-rhel9 --prefix "$HOME/.local" --component facts-tool
```

The helper installs build dependencies, selects GCC Toolset 15, builds the native
binary, and runs its tests. It uses compatible installed Boost and JSON headers
or fetches checksum-verified Boost 1.83.0 and nlohmann/json 3.11.3 sources into
`.deps/api-headers`. No Boost binary libraries are required. This also handles
hosts whose distribution Boost package is older than the required version 1.74.

Generated OpenAPI bindings are included in the checkout. Normal builds do not
require code generation; test-enabled builds install the OpenAPI validation
tools through the helper's existing test requirements step. `SKIP_TESTS=1`
keeps those Python development tools out of the native build.

Dependency installation uses `sudo` when needed. `SKIP_DEPS=1` skips `dnf`, while
still preparing source dependencies; `SKIP_TESTS=1` skips the tests. Use
`BOOST_SOURCE_DIR` and `JSON_SOURCE_DIR` for predownloaded headers, and see
[cached headers and offline builds](../01-introduction/03-installation.md#cached-headers-and-offline-builds)
for cache locations and the remaining offline requirements.

### macOS with Homebrew

```bash
cd skeletons/facts-tool
brew install cmake ninja llvm boost nlohmann-json
cmake -G Ninja -B build \
  -DCMAKE_PREFIX_PATH="$(brew --prefix llvm);$(brew --prefix)" \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++" .
cmake --build build
./build/facts-tool serve --help
cmake --install build --prefix "$HOME/.local" --component facts-tool
```

The API needs [Boost](https://formulae.brew.sh/formula/boost) and
[nlohmann-json](https://formulae.brew.sh/formula/nlohmann-json). Use Homebrew LLVM
rather than Apple's system compiler. Foreground and daemon servers run on macOS;
inotify watching requires a Linux server.

### Verify the installed binary

```bash
"$HOME/.local/bin/facts-tool" serve --help
"$HOME/.local/bin/facts-tool" config show
```

The `facts-tool` installation component installs the binary in `bin/` under your
chosen prefix, with the editable OpenAPI YAML in `share/facts-tool/openapi/`.
The running server uses its embedded contract; changing installed YAML requires
regenerating and rebuilding from source. See
[OpenAPI contract and generated code](08-openapi-contract.md).
The installation does not bundle linked LLVM/Clang libraries. Keep the matching
Homebrew LLVM installation on macOS, or `clang-libs` and `llvm-libs` on RHEL-family
hosts. Build for the target platform and use compatible runtime versions when
copying a binary to another host. Check linked libraries with `otool -L` on macOS
or `ldd` on Linux if startup reports a missing library.

## Install the Python REST wrapper

From the **repository root**, create a Python 3.12 environment and install the
SDK's `rest` extra:

```bash
python3.12 -m venv .venv-api
.venv-api/bin/python -m pip install './skeletons/facts-tool/python[rest]'
.venv-api/bin/python -c 'from facts_tool.rest import Client, AsyncClient'
```

This installs HTTPX alongside the SDK. For an editable development installation,
add `-e` to the pip command, or run `uv sync --locked --extra rest` inside
`skeletons/facts-tool/python`. Omitting `[rest]` installs only the local SQLite
query SDK and its standard-library runtime.

To distribute a wheel, build from the SDK directory:

```bash
cd skeletons/facts-tool/python
uv build --out-dir /path/to/dist
```

On the client host, install the generated wheel with its extra, replacing
`VERSION` with the filename's actual version:

```bash
python3.12 -m venv /path/to/client-venv
/path/to/client-venv/bin/python -m pip install \
  '/path/to/dist/facts_tool_query-VERSION-py3-none-any.whl[rest]'
```

Use a source checkout or wheel containing this feature; a bare package name
is not a substitute for installing this revision.

## Verify a running server

Use [Deployment and operation](06-deployment.md) to start the server. Read its
allocated port from the startup message or server YAML, then call `/health` or
run the [Python REST client example](../05-python-sdk/11-rest-client.md).
Installing the SDK does not start a server.
