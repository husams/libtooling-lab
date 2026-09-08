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
environment with checkout `PYTHONPATH` removed.
