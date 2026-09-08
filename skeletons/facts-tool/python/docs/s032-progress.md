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

The macOS gate used Homebrew LLVM 22 (`/opt/homebrew/opt/llvm/bin/clang++`),
the current `build-s032/facts-tool`, `uv run pytest -q tests/test_distributions.py`
(1 passed), and the installed wheel and sdist smoke. Python 3.13, Linux, and
RHEL were unavailable in this session and are recorded as unavailable rather
than inferred. S-028 remains separately owned; this workflow consumes its
native-first/public-reader disposition from the
[facts-tool reasoning skill](../../.agents/skills/facts-tool-code-reasoning/SKILL.md)
and does not claim S-028 acceptance.
