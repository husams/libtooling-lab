import json
from pathlib import Path
from typing import Any

import pytest

from facts_tool import FactsToolError, MatchResults, load_match_results


def _binding(kind: str, name: str | None = "f") -> dict[str, object]:
    return {
        "node_kind": kind,
        "name": name,
        "usr": "c:@F@f" if name else None,
        "location": {
            "path": "/work/include/api.hpp",
            "line": 2,
            "column": 3,
            "offset": 9,
        },
        "range": {"path": "/work/include/api.hpp", "offset": 9, "size": 1},
        "location_unavailable_reason": None,
    }


def _payload() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "matcher": "callExpr().bind(\"call\")",
        "sources": ["/work/one.cpp", "/work/two.cpp"],
        "complete": True,
        "facts_committed": True,
        "index_committed": True,
        "matches": [
            {
                "translation_unit": tu,
                "relation_kind": "calls" if index else None,
                "bindings": {
                    "symbol": _binding("FunctionDecl"),
                    "expression": _binding("Expr"),
                    "call": _binding("CallExpr"),
                    "callee": _binding("FunctionDecl"),
                    "source": _binding("FunctionDecl"),
                    "target": _binding("FunctionDecl"),
                    "site": _binding("CallExpr"),
                },
            }
            for index, tu in enumerate(("/work/one.cpp", "/work/two.cpp"))
        ],
    }


def test_multi_tu_results_keep_header_occurrences_and_bindings() -> None:
    results = MatchResults.from_dict(_payload())
    assert results.matches[0].translation_unit != results.matches[1].translation_unit
    location = results.matches[0].bindings["symbol"].location
    assert location is not None and location.path.endswith("api.hpp")


def test_empty_results_round_trip_and_immutable_collections() -> None:
    payload = _payload()
    payload["matches"] = []
    results = MatchResults.from_dict(payload)
    assert results.matches == ()
    assert MatchResults.from_json(results.to_json()).to_dict() == results.to_dict()


def test_malformed_json_and_schema_fail_with_stable_error() -> None:
    with pytest.raises(FactsToolError, match="E_SCHEMA"):
        MatchResults.from_json("{")
    for payload in ({**_payload(), "schema_version": 2}, {**_payload(), "complete": 1}):
        with pytest.raises(FactsToolError, match="E_SCHEMA"):
            MatchResults.from_dict(payload)


def test_invalid_coordinate_and_file_loader(tmp_path: Path) -> None:
    payload = _payload()
    matches = payload["matches"]
    assert isinstance(matches, list)
    location = matches[0]["bindings"]["symbol"]["location"]
    location["offset"] = -1
    with pytest.raises(FactsToolError, match="E_SCHEMA"):
        MatchResults.from_dict(payload)
    path = tmp_path / "matches.json"
    path.write_text(json.dumps(_payload()), encoding="utf-8")
    assert load_match_results(path).to_dict() == _payload()


def test_location_availability_and_range_consistency_are_validated() -> None:
    for location, reason in ((None, None), ({}, "missing")):
        payload = _payload()
        binding = payload["matches"][0]["bindings"]["symbol"]
        binding["location"], binding["location_unavailable_reason"] = location, reason
        with pytest.raises(FactsToolError, match="E_SCHEMA"):
            MatchResults.from_dict(payload)
    payload = _payload()
    binding = payload["matches"][0]["bindings"]["symbol"]
    binding["range"]["path"] = "/work/other.hpp"
    with pytest.raises(FactsToolError, match="E_SCHEMA"):
        MatchResults.from_dict(payload)
