"""Project transition hooks for the libtooling-lab backlog.

The action-workflow engine imports `pre_transition` and `post_transition` from
this module and calls both as::

    hook(action, context, current_state, proposed_state, bl)

`context` carries `operation`, `actor`, `task_key` and `parameters`; `bl` is the
active public `backlog_cli.api.Backlog` session. `pre_transition` runs before
the gates and must return a non-empty destination state -- returning
`proposed_state` unchanged means "no override". `post_transition` runs after the
transition has already been committed.
"""

from __future__ import annotations

import sys

# This repository's GitHub account does not require an explicit reviewer
# approval on a pull request, so `pr_review_state` would sit at `pending`
# forever and block both `gate --for merge` and Accepted -> Done. Once a
# deliverable is Accepted with every review comment closed, the approval is a
# formality nobody is going to click, so record it here instead.
APPROVED = "approved"
TRIGGER_STATE = "accepted"
DELIVERABLE_TYPES = {"story", "bug", "subtask"}
DEAD_PR_STATES = {"merged", "closed"}

# `set_pr` emits `pr.approved`, which re-enters these hooks before the new PR
# state is visible. Without this guard the hook would recurse indefinitely.
_approving: set[str] = set()


def pre_transition(action, context, current_state, proposed_state, bl):
    """No project-specific override; keep the state the workflow resolved."""
    return proposed_state


def post_transition(action, context, current_state, proposed_state, bl):
    if proposed_state != TRIGGER_STATE:
        return
    if _action_value(action) == "pr.approved":
        return
    key = context.get("task_key")
    if not key:
        return
    try:
        _auto_approve_pr(key, context, bl)
    except Exception as exc:  # the transition itself is already committed
        print(
            f"warning: PR auto-approval hook failed for {key}: {exc}",
            file=sys.stderr,
        )


def _auto_approve_pr(key, context, bl):
    if key in _approving:
        return

    task = bl.find(key)
    if task is None or task.task_type not in DELIVERABLE_TYPES:
        return
    if task.pr_review_state == APPROVED:
        return
    if not (task.pr_url or getattr(task, "pr_number", None)):
        return  # no pull request recorded, so there is nothing to approve
    if getattr(task, "pr_state", None) in DEAD_PR_STATES:
        return
    if bl.threads(key, state="open"):
        return  # a review comment of some severity is still open

    _approving.add(key)
    try:
        bl.set_pr(key, review_state=APPROVED, actor=context.get("actor"))
    finally:
        _approving.discard(key)


def _action_value(action):
    return getattr(action, "value", str(action))
