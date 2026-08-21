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
#   * SQLite. The storage layer uses RETURNING (SQLite >= 3.35); RHEL 9 ships
#     3.34.1. A static libsqlite3.a is built from the amalgamation into a local
#     prefix and handed to find_package(SQLite3) — nothing under /usr is
#     touched.
#   * Clang/LLVM. clang-devel + llvm-devel supply libclang-cpp, libLLVM, and
#     the ClangConfig.cmake/LLVMConfig.cmake that find_package(Clang) needs.
#     They stay dynamically linked: to run the binary elsewhere the host needs
#     `dnf install -y clang-libs llvm-libs`.
#
# Knobs (env vars):
#   GCC_TOOLSET             gcc-toolset major for C++23 (default 15; 14 is the
#                           minimum — std::ranges::to landed in libstdc++ 14).
#   SQLITE_FROM             where to get SQLite: 'amalgamation' (zip from
#                           sqlite.org), 'git' (GitHub source), or 'auto'
#                           (default: try the zip, fall back to GitHub if the
#                           download is blocked).
#   SQLITE_AMALGAMATION_URL amalgamation zip URL (default 3.45.1).
#   SQLITE_GIT_URL          SQLite git mirror (default github.com/sqlite/sqlite).
#   SQLITE_GIT_TAG          tag to build (default version-3.45.1).
#   SQLITE_PREFIX           where the local SQLite lands
#                           (default <root>/.deps/sqlite).
#   FORCE_SQLITE=1          rebuild the local SQLite even if present.
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
SQLITE_FROM="${SQLITE_FROM:-auto}"
SQLITE_AMALGAMATION_URL="${SQLITE_AMALGAMATION_URL:-https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip}"
SQLITE_GIT_URL="${SQLITE_GIT_URL:-https://github.com/sqlite/sqlite.git}"
SQLITE_GIT_TAG="${SQLITE_GIT_TAG:-version-3.45.1}"
SQLITE_PREFIX="${SQLITE_PREFIX:-$FACTS_ROOT/.deps/sqlite}"
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
    cmake ninja-build make git tar xz unzip which \
    python3 python3-pip \
    clang-devel llvm-devel clang-libs llvm-libs
fi

if [ "${DEPS_ONLY:-0}" = "1" ]; then
  echo "==> DEPS_ONLY: dependencies installed; skipping build"
  exit 0
fi

TOOLSET_ENABLE="/opt/rh/gcc-toolset-${GCC_TOOLSET}/enable"
[ -f "$TOOLSET_ENABLE" ] || { echo "error: $TOOLSET_ENABLE missing (install gcc-toolset-${GCC_TOOLSET})" >&2; exit 1; }

# --- SQLite >= 3.35 (RHEL 9 ships 3.34.1, which has no RETURNING) ------------
# Fetch the SQLite sources into $1 (leaving sqlite3.c + sqlite3.h there) from the
# amalgamation zip or the GitHub mirror. 'auto' tries the zip and falls back to
# GitHub when the download is blocked (e.g. a proxy returns 403).
fetch_sqlite_src() {
  local out="$1" got_zip=0
  if [ "$SQLITE_FROM" = amalgamation ] || [ "$SQLITE_FROM" = auto ]; then
    if curl -fsSL -o "$out/sqlite.zip" "$SQLITE_AMALGAMATION_URL"; then
      ( cd "$out" && unzip -q sqlite.zip \
        && cp sqlite-amalgamation-*/sqlite3.c sqlite-amalgamation-*/sqlite3.h \
              sqlite-amalgamation-*/sqlite3ext.h . )
      got_zip=1
    elif [ "$SQLITE_FROM" = amalgamation ]; then
      echo "error: could not download $SQLITE_AMALGAMATION_URL" >&2; return 1
    else
      echo "   amalgamation download blocked — generating from GitHub source"
    fi
  fi
  if [ "$got_zip" -eq 0 ]; then
    # GitHub mirror: build the amalgamation from the tag (needs tcl to run
    # tool/mksqlite3c.tcl via ./configure && make sqlite3.c).
    [ "${SKIP_DEPS:-0}" = "1" ] || $SUDO dnf -y install git tcl file >/dev/null
    git clone --depth 1 -b "$SQLITE_GIT_TAG" "$SQLITE_GIT_URL" "$out/src"
    ( cd "$out/src" && ./configure >/dev/null && make sqlite3.c >/dev/null )
    cp "$out/src/sqlite3.c" "$out/src/sqlite3.h" "$out/src/sqlite3ext.h" "$out/"
  fi
}

if [ ! -f "$SQLITE_PREFIX/lib/libsqlite3.a" ] || [ "${FORCE_SQLITE:-0}" = "1" ]; then
  echo "==> building $SQLITE_PREFIX/lib/libsqlite3.a (SQLITE_FROM=$SQLITE_FROM)"
  tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
  fetch_sqlite_src "$tmp"
  # SQLITE_USE_URI keeps URI filenames working the way macOS's system SQLite
  # does, so a database path is interpreted identically on both platforms.
  gcc -O2 -fPIC -DSQLITE_ENABLE_FTS5 -DSQLITE_ENABLE_JSON1 -DSQLITE_ENABLE_RTREE \
      -DSQLITE_USE_URI=1 -DSQLITE_OMIT_LOAD_EXTENSION=1 \
      -c "$tmp/sqlite3.c" -o "$tmp/sqlite3.o"
  mkdir -p "$SQLITE_PREFIX/lib" "$SQLITE_PREFIX/include"
  ar rcs "$SQLITE_PREFIX/lib/libsqlite3.a" "$tmp/sqlite3.o"
  install -m 0644 "$tmp/sqlite3.h" "$tmp/sqlite3ext.h" "$SQLITE_PREFIX/include/"
  rm -rf "$tmp"; trap - EXIT
else
  echo "==> $SQLITE_PREFIX/lib/libsqlite3.a already present (FORCE_SQLITE=1 to rebuild)"
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
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DLLVM_DIR="$LLVM_CMAKEDIR" -DClang_DIR="$CLANG_CMAKEDIR" \
  -DSQLite3_INCLUDE_DIR="$SQLITE_PREFIX/include" \
  -DSQLite3_LIBRARY="$SQLITE_PREFIX/lib/libsqlite3.a" \
  -DPython3_EXECUTABLE="$PYTHON" \
  -DBUILD_TESTING="$([ "${SKIP_TESTS:-0}" = "1" ] && echo OFF || echo ON)"
cmake --build "$BUILD_DIR" -j"$JOBS"

if [ "${SKIP_TESTS:-0}" != "1" ]; then
  echo "==> running ctest"
  ctest --test-dir "$BUILD_DIR" --output-on-failure -j"$JOBS"
fi

echo
echo "==> built: $BUILD_DIR/facts-tool"
# --help still opens the databases named by the default options, so smoke-test
# from a scratch directory instead of dropping facts.db/project.db in the repo.
smoke="$(mktemp -d)"
( cd "$smoke" && "$BUILD_DIR/facts-tool" --help ) >/dev/null 2>&1 \
  && echo "    facts-tool runs OK"
rm -rf "$smoke"
echo "    to run on another RHEL 9 host: copy the binary + 'dnf install -y clang-libs llvm-libs'"
