# S-032 implementation progress

- [x] Define the installed native/SDK workflow over one isolated paired store.
- [x] Add installed wheel/sdist acceptance for native graph reuse, symbol
  search, field writers, ancestors, expressions, and bounded function,
  method, and class definition regions.
- [x] Record concise acceptance metrics and keep query output bounded.
- [x] Reconcile skill and call-graph guidance with public SDK readers,
  freshness/coverage flags, and S-028 ownership.

The acceptance harness is `scripts/agent_acceptance.py`; the distribution gate
runs it once from a clean wheel and once from a clean source-distribution
environment with checkout `PYTHONPATH` removed. Both runs consume the same
current-native schema13 pair, including native expression/source capture and a
single persisted graph run, and read that exact run with `get(run_id)`.

On macOS 15 with Python 3.12.10, the exact native gate was
`cmake -G Ninja -B build-s032 -DBUILD_TESTING=OFF -DCMAKE_PREFIX_PATH="$(brew --prefix llvm)" -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++" .`
followed by `cmake --build build-s032 --target facts-tool`; the executable is
required explicitly through `FACTS_TOOL_NATIVE`. The exact SDK gates were
`uv run pytest -q` (91 passed), `uv run python scripts/run_installed_bdd.py`
(42 passed), `uv run pytest -q tests/test_distributions.py` (1 passed),
`uv run ruff check .`, `uv run mypy --strict src/facts_tool`,
`uv run python scripts/check_import_boundaries.py`, and `uv build --out-dir
/tmp/s032-agent-dist` (all passed). Python 3.13, Linux, and RHEL were
unavailable in this session and are recorded as unavailable rather than
inferred. The isolated `cpp-senior-engineer` dispatch was also unavailable
because the agent runtime reported its thread limit; no profile result is
claimed, and the native pair plus wheel/sdist transcripts are preserved under
`/tmp/s032-agent-evidence/`. S-028 remains separately owned; the auditable
Backlog handoff is S-032 note 7923, which records S-028 as Ready with no owner,
reviewer claude, and no PR/artifact. This workflow consumes its
native-first/public-reader disposition from the
[facts-tool reasoning skill](../../.agents/skills/facts-tool-code-reasoning/SKILL.md)
and does not claim S-028 acceptance.
