from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from support.native_commands import prepare_native_pair


@dataclass(frozen=True)
class NativeAgentPair:
    facts: Path
    project: Path
    source: Path
    run_id: int
    setup_calls: int
    setup_output_chars: int


def build_native_agent_pair(root: Path) -> NativeAgentPair:
    facts, project, source, run_id, chars = prepare_native_pair(root)
    return NativeAgentPair(facts, project, source, run_id, 7, chars)
