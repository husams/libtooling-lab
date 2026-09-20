#!/usr/bin/env bash
# Sourced by build-rhel9.sh. Dependency versions and downloads live in CMake.
API_HEADER_CACHE_DIR="${API_HEADER_CACHE_DIR:-$FACTS_ROOT/.deps/api-headers}"
mkdir -p "$API_HEADER_CACHE_DIR"
API_HEADER_CACHE_DIR="$(cd "$API_HEADER_CACHE_DIR" && pwd)"
HEADER_SOURCE_ARGS=()
if [ -n "${BOOST_SOURCE_DIR:-}" ]; then
  HEADER_SOURCE_ARGS+=(-DFETCHCONTENT_SOURCE_DIR_BOOST="$BOOST_SOURCE_DIR")
fi
if [ -n "${JSON_SOURCE_DIR:-}" ]; then
  HEADER_SOURCE_ARGS+=(-DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON="$JSON_SOURCE_DIR")
fi

echo "==> preparing REST API headers in $API_HEADER_CACHE_DIR"
cmake -S "$SCRIPT_DIR/dependencies" -B "$API_HEADER_CACHE_DIR/configure" \
  -U 'FACTS_*_INCLUDE_DIR' \
  -U FETCHCONTENT_SOURCE_DIR_BOOST -U FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON \
  -DFETCHCONTENT_BASE_DIR="$API_HEADER_CACHE_DIR" \
  "${HEADER_SOURCE_ARGS[@]}"

# Use exactly the validated headers selected above, even with a reused build.
HEADER_ARGS=(-U FETCHCONTENT_SOURCE_DIR_BOOST
             -U FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON
             -C "$API_HEADER_CACHE_DIR/configure/api-header-cache.cmake")
