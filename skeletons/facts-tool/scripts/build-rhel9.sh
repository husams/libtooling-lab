#!/usr/bin/env bash
# build-rhel9.sh — install dependencies (incl. a modern SQLite) and build
# facts-tool on RHEL 9.x / AlmaLinux 9 / Rocky 9. Run it from anywhere; it
# locates the skeleton.
#
#   ./scripts/build-rhel9.sh              # install deps, build, run ctest
#   DEPS_ONLY=1 ./scripts/build-rhel9.sh  # install dependencies, no build
#   SKIP_TESTS=1 ./scripts/build-rhel9.sh # build only, no pytest venv, no ctest
#
# Produces: <root>/build-rhel9/facts-tool.
#
# Why the extra plumbing (RHEL 9 differs from the macOS lab environment):
#   * C++23. The skeleton uses std::expected and std::ranges::to, so it needs
#     gcc >= 14; RHEL 9's system gcc is 11. gcc-toolset-15 supplies the
#     compiler and libstdc++ headers. The toolset links the new libstdc++
#     symbols statically and keeps libstdc++.so.6 dynamic, so the ABI stays
#     compatible with libLLVM.
#   * SQLite. The storage layer uses RETURNING (SQLite >= 3.35) and links
#     SQLite statically. If the host already has a static libsqlite3.a that is
#     new enough, the script uses it as-is; only when that is missing does it
#     fetch the official amalgamation for CMake to compile, and the sources are
#     cached under .deps/ so later runs neither re-download nor re-fetch.
#     Stock RHEL 9 has neither (sqlite-devel is 3.34.1 and ships no .a), so the
#     first run there builds SQLite once.
#   * Clang/LLVM. clang-devel + llvm-devel supply libclang-cpp, libLLVM, and
#     the ClangConfig.cmake/LLVMConfig.cmake that find_package(Clang) needs.
#     They stay dynamically linked: to run the binary elsewhere the host needs
#     `dnf install -y clang-libs llvm-libs`.
#
# Knobs (env vars):
#   GCC_TOOLSET             gcc-toolset major for C++23 (default 15; 14 is the
#                           minimum — std::ranges::to landed in libstdc++ 14).
#   SQLITE_SOURCE_DIR       unpacked sqlite amalgamation to build from
#                           (default <root>/.deps/sqlite-amalgamation; fetched
#                           on first use, reused afterwards).
#   SQLITE_AMALGAMATION_URL amalgamation zip URL (default 3.53.4).
#   YAML_SOURCE_DIR      unpacked yaml-cpp 0.9.0 source override/cache.
#   FORCE_SQLITE=1          re-fetch the amalgamation even if cached, and
#                           ignore any system libsqlite3.a.
#   BUILD_DIR               cmake build dir (default <root>/build-rhel9).
#   VENV_DIR                venv holding pytest/pytest-bdd for the e2e suite
#                           (default <root>/.venv-rhel9).
#   JOBS                    parallel build jobs (default: nproc).
#   SKIP_DEPS=1             skip dnf installs (deps already present).
#   DEPS_ONLY=1             install dependencies, then exit (no build).
#   SKIP_TESTS=1            configure with BUILD_TESTING=OFF and skip ctest.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FACTS_ROOT="$(dirname "$SCRIPT_DIR")"

GCC_TOOLSET="${GCC_TOOLSET:-15}"
SQLITE_SOURCE_DIR="${SQLITE_SOURCE_DIR:-$FACTS_ROOT/.deps/sqlite-amalgamation}"
YAML_SOURCE_DIR="${YAML_SOURCE_DIR:-$FACTS_ROOT/.deps/yaml-cpp-0.9.0}"
YAML_URL="${YAML_URL:-https://github.com/jbeder/yaml-cpp/releases/download/yaml-cpp-0.9.0/yaml-cpp-yaml-cpp-0.9.0.tar.gz}"
YAML_SHA256="298593d9c440fd9034b8b193d96318b76d49bc97c6ceadb7b0836edf0b6d7539"
SQLITE_AMALGAMATION_URL="${SQLITE_AMALGAMATION_URL:-https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip}"
BUILD_DIR="${BUILD_DIR:-$FACTS_ROOT/build-rhel9}"
VENV_DIR="${VENV_DIR:-$FACTS_ROOT/.venv-rhel9}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

SUDO=""
[ "$(id -u)" -eq 0 ] || SUDO="sudo"

# --- dependencies ------------------------------------------------------------
if [ "${SKIP_DEPS:-0}" != "1" ]; then
  echo "==> installing build dependencies (dnf)"
  # CRB / CodeReady Builder (some deps live there). Repo id differs by distro;
  # try each, never fail the run if it cannot be toggled.
  $SUDO dnf config-manager --set-enabled crb 2>/dev/null \
    || $SUDO dnf config-manager --set-enabled "codeready-builder-for-rhel-9-$(arch)-rpms" 2>/dev/null \
    || $SUDO subscription-manager repos --enable "codeready-builder-for-rhel-9-$(arch)-rpms" 2>/dev/null \
    || $SUDO crb enable 2>/dev/null \
    || echo "   (could not enable CRB automatically — continuing)"
  $SUDO dnf -y install \
    "gcc-toolset-${GCC_TOOLSET}" "gcc-toolset-${GCC_TOOLSET}-libstdc++-devel" \
    cmake ninja-build make git tar xz unzip which curl \
    python3 python3-pip \
    clang-devel llvm-devel clang-libs llvm-libs
fi

YAML_ARGS=()
if [ ! -f "$YAML_SOURCE_DIR/CMakeLists.txt" ]; then
  echo "==> fetching pinned yaml-cpp 0.9.0 into $YAML_SOURCE_DIR"
  tmp_yaml="$(mktemp -d)"
  curl -fsSL -o "$tmp_yaml/yaml.tar.gz" "$YAML_URL"
  echo "$YAML_SHA256  $tmp_yaml/yaml.tar.gz" | sha256sum -c -
  mkdir -p "$(dirname "$YAML_SOURCE_DIR")"
  tar -xzf "$tmp_yaml/yaml.tar.gz" -C "$tmp_yaml"
  mv "$tmp_yaml/yaml-cpp-yaml-cpp-0.9.0" "$YAML_SOURCE_DIR"
  rm -rf "$tmp_yaml"
else
  echo "==> reusing pinned yaml-cpp 0.9.0 in $YAML_SOURCE_DIR"
fi
YAML_ARGS=(-DFETCHCONTENT_SOURCE_DIR_YAML_CPP="$YAML_SOURCE_DIR")

if [ "${DEPS_ONLY:-0}" = "1" ]; then
  echo "==> DEPS_ONLY: dependencies installed; skipping build"
  exit 0
fi

TOOLSET_ENABLE="/opt/rh/gcc-toolset-${GCC_TOOLSET}/enable"
[ -f "$TOOLSET_ENABLE" ] || { echo "error: $TOOLSET_ENABLE missing (install gcc-toolset-${GCC_TOOLSET})" >&2; exit 1; }

# --- SQLite >= 3.35, statically linked ---------------------------------------
# Nothing is built here if the host can already satisfy it: a system
# libsqlite3.a that is new enough is linked directly. Otherwise CMake compiles
# the amalgamation, whose sources are cached in $SQLITE_SOURCE_DIR so only the
# first run fetches anything.
SQLITE_ARGS=()

system_static_sqlite() {
  local hdr=/usr/include/sqlite3.h lib version
  [ "${FORCE_SQLITE:-0}" = "1" ] && return 1
  [ -f "$hdr" ] || return 1
  for lib in /usr/lib64/libsqlite3.a /usr/lib/libsqlite3.a; do
    [ -f "$lib" ] && break || lib=""
  done
  [ -n "$lib" ] || return 1
  version="$(sed -n 's/^#define SQLITE_VERSION_NUMBER  *\([0-9]*\).*/\1/p' "$hdr" | head -1)"
  [ -n "$version" ] && [ "$version" -ge 3035000 ] || return 1
  SYSTEM_SQLITE_LIB="$lib"
  return 0
}

if system_static_sqlite; then
  echo "==> using the installed static SQLite ($SYSTEM_SQLITE_LIB)"
  SQLITE_ARGS=(-DFACTS_SYSTEM_SQLITE=ON
               -DSQLite3_INCLUDE_DIR=/usr/include
               -DSQLite3_LIBRARY="$SYSTEM_SQLITE_LIB")
else
  if [ ! -f "$SQLITE_SOURCE_DIR/sqlite3.c" ] || [ "${FORCE_SQLITE:-0}" = "1" ]; then
    echo "==> fetching the SQLite amalgamation into $SQLITE_SOURCE_DIR"
    tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
    if curl -fsSL -o "$tmp/sqlite.zip" "$SQLITE_AMALGAMATION_URL"; then
      ( cd "$tmp" && unzip -q sqlite.zip )
      rm -rf "$SQLITE_SOURCE_DIR"
      mkdir -p "$(dirname "$SQLITE_SOURCE_DIR")"
      mv "$tmp"/sqlite-amalgamation-*/ "$SQLITE_SOURCE_DIR"
    else
      # No local copy: let CMake's FetchContent do the download itself.
      echo "   could not download $SQLITE_AMALGAMATION_URL — leaving it to cmake"
    fi
    rm -rf "$tmp"; trap - EXIT
  else
    echo "==> reusing the cached SQLite amalgamation in $SQLITE_SOURCE_DIR"
  fi
  if [ -f "$SQLITE_SOURCE_DIR/sqlite3.c" ]; then
    SQLITE_ARGS=(-DFETCHCONTENT_SOURCE_DIR_SQLITE3="$SQLITE_SOURCE_DIR")
  fi
fi

# --- python venv for the pytest-bdd e2e suite --------------------------------
# CMake refuses to configure with BUILD_TESTING=ON unless pytest and pytest-bdd
# import, so the venv is created before configure and handed over as Python3.
PYTHON="$(command -v python3)"
if [ "${SKIP_TESTS:-0}" != "1" ]; then
  if [ ! -x "$VENV_DIR/bin/python3" ]; then
    echo "==> creating venv $VENV_DIR"
    python3 -m venv "$VENV_DIR"
  fi
  echo "==> installing e2e test requirements"
  "$VENV_DIR/bin/python3" -m pip install --quiet --upgrade pip
  "$VENV_DIR/bin/python3" -m pip install --quiet -r "$FACTS_ROOT/tests/e2e/requirements.txt"
  PYTHON="$VENV_DIR/bin/python3"
fi

# --- build facts-tool --------------------------------------------------------
echo "==> building facts-tool (gcc-toolset-${GCC_TOOLSET}; static SQLite, dynamic Clang/LLVM)"
# shellcheck disable=SC1090
source "$TOOLSET_ENABLE"
# find_package(Clang) discovery: point cmake at this LLVM's config packages so
# the LibTooling targets (clang-cpp + LLVM) resolve.
LLVM_CMAKEDIR="$(llvm-config --cmakedir)"
CLANG_CMAKEDIR="$(dirname "$LLVM_CMAKEDIR")/clang"
cmake -G Ninja -S "$FACTS_ROOT" -B "$BUILD_DIR" \
  "${SQLITE_ARGS[@]}" \
  "${YAML_ARGS[@]}" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DLLVM_DIR="$LLVM_CMAKEDIR" -DClang_DIR="$CLANG_CMAKEDIR" \
  -DPython3_EXECUTABLE="$PYTHON" \
  -DBUILD_TESTING="$([ "${SKIP_TESTS:-0}" = "1" ] && echo OFF || echo ON)"
cmake --build "$BUILD_DIR" -j"$JOBS"

if [ "${SKIP_TESTS:-0}" != "1" ]; then
  echo "==> running ctest"
  ctest --test-dir "$BUILD_DIR" --output-on-failure -j"$JOBS"
fi

echo
echo "==> built: $BUILD_DIR/facts-tool"
ldd "$BUILD_DIR/facts-tool" | grep -q 'libyaml-cpp' && {
  echo "error: yaml-cpp is dynamically linked" >&2
  exit 1
} || echo "    yaml-cpp: static (no libyaml-cpp.so dependency)"
# --help still opens the databases named by the default options, so smoke-test
# from a scratch directory instead of dropping facts.db/project.db in the repo.
smoke="$(mktemp -d)"
( cd "$smoke" && "$BUILD_DIR/facts-tool" --help ) >/dev/null 2>&1 \
  && echo "    facts-tool runs OK"
rm -rf "$smoke"
echo "    to run on another RHEL 9 host: copy the binary + 'dnf install -y clang-libs llvm-libs'"
