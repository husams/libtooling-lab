import json
from collections.abc import Iterator, Mapping
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .match_models import MatchResult


@dataclass(frozen=True)
class MatchResults:
    schema_version: int
    matcher: str
    sources: tuple[str, ...]
    complete: bool
    facts_committed: bool
    index_committed: bool
    matches: tuple[MatchResult, ...]

    def __post_init__(self) -> None:
        object.__setattr__(self, "sources", tuple(self.sources))
        object.__setattr__(self, "matches", tuple(self.matches))

    def __iter__(self) -> Iterator[MatchResult]:
        return iter(self.matches)

    def __len__(self) -> int:
        return len(self.matches)

    @classmethod
    def from_dict(cls, payload: Mapping[str, Any]) -> "MatchResults":
        from .match_decode import decode

        return decode(payload)

    @classmethod
    def from_json(cls, payload: str | bytes) -> "MatchResults":
        from .match_decode import decode, decode_json

        value = decode_json(payload)
        if not isinstance(value, Mapping):
            from .errors import fail

            fail("E_SCHEMA", "match results JSON root must be an object")
        return decode(value)

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": self.schema_version,
            "matcher": self.matcher,
            "sources": list(self.sources),
            "complete": self.complete,
            "facts_committed": self.facts_committed,
            "index_committed": self.index_committed,
            "matches": [match.to_dict() for match in self.matches],
        }

    def to_json(self) -> str:
        return json.dumps(self.to_dict(), ensure_ascii=False, sort_keys=True)


def load_match_results(path: str | Path) -> MatchResults:
    source = Path(path)
    try:
        payload = source.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        from .errors import FactsToolError

        raise FactsToolError(
            "E_SOURCE", f"cannot read match results {source}: {exc}"
        ) from exc
    return MatchResults.from_json(payload)
