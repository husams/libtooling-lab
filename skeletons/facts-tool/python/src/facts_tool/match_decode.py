from __future__ import annotations

import json
from collections.abc import Mapping
from typing import Any

from .errors import FactsToolError, fail
from .match_decode_nodes import match
from .match_results import MatchResults
from .match_validation import (
    array,
    as_mapping,
    boolean,
    required,
    string,
)


def decode_json(payload: str | bytes) -> Any:
    try:
        if not isinstance(payload, (str, bytes)):
            fail("E_SCHEMA", "match results JSON must be text or UTF-8 bytes")
        return json.loads(payload)
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        raise FactsToolError("E_SCHEMA", f"invalid match results JSON: {exc}") from exc


def decode(payload: Mapping[str, Any]) -> MatchResults:
    data = as_mapping(payload, "match results")
    version = required(data, "schema_version")
    if type(version) is not int or version != 1:
        fail("E_SCHEMA", "unsupported match results schema_version")
    sources = tuple(
        string(item, f"sources[{index}]", absolute=True)
        for index, item in enumerate(array(required(data, "sources"), "sources"))
    )
    matches = tuple(
        match(item, index)
        for index, item in enumerate(array(required(data, "matches"), "matches"))
    )
    return MatchResults(
        1,
        string(required(data, "matcher"), "matcher"),
        sources,
        boolean(required(data, "complete"), "complete"),
        boolean(required(data, "facts_committed"), "facts_committed"),
        boolean(required(data, "index_committed"), "index_committed"),
        matches,
    )
