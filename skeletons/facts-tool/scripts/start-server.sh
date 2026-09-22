#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
exec "${FACTS_SERVER_PYTHON:-$repo_root/.server-runtime/venv/bin/python}" \
  "$repo_root/scripts/server-control.py" start "$@"
