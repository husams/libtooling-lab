# B-014 acceptance evidence

Both runs use the same test assets and the same host; only the implementation differs.

| Run | Implementation | Scenarios | New symlink scenarios | Log |
| --- | --- | --- | --- | --- |
| Baseline (unfixed) | `b0b28bf` (current `main`), `src/` untouched | 149 collected, 2 selected | **2 failed** — `cannot store project configuration: Invalid argument` | `baseline-unfixed.log` |
| Fixed | `b0b28bf` + the B-014 working-tree change | 149 collected, 2 selected | **2 passed** | `fixed.log` |

Baseline worktree: `~/.claude/worktrees/libtooling-lab/b-014-baseline` (detached at `b0b28bf`,
with only `tests/e2e/{features/project_import.feature, steps/project_import_steps.py,
support/scenario.py, conftest.py}` copied in — `git status -- src` is empty there).
Implementation worktree: `~/.claude/worktrees/libtooling-lab/fix-b-014-symlinked-compilation-sources`.

## Full gate on the fix — `bash scripts/run-e2e.sh build`

- `full-gate/bdd.xml`: `tests="149" failures="0" errors="0" skipped="1"` — 148 passed, 1 skipped.
  The skip is the pre-existing GNU-driver prefix-header scenario (no GNU driver on macOS).
- `full-gate/ctest.xml`, `full-gate/ctest.log`: `facts-tool-cli-contract` and `facts-tool-e2e` both passed (2/2).
- `ctest-all.log`: whole CTest suite, 21/22 passed.
- `fact-store-baseline.log`: `fact-store` aborts identically on unmodified `b0b28bf`, so that
  failure is pre-existing and unrelated to B-014.

## Correction to the earlier Backlog note

The "149 scenarios" figure was correct for the branch as it stands, but at the time it was
written the reviewer's tree predated `b0b28bf` and collected 82 (81 passed + 1 skipped).
149 = those 82 plus the 67 catalog cases that landed in `b0b28bf`. Both numbers above are
from the same revision, so they now agree.
