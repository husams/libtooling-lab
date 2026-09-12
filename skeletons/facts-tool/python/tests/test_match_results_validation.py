import json
from dataclasses import FrozenInstanceError

import pytest
from test_match_results import _payload

from facts_tool import FactsToolError, MatchResults


@pytest.mark.parametrize("field", ["location", "range"])
def test_binding_paths_must_be_absolute(field):
    payload = _payload()
    payload["matches"][0]["bindings"]["symbol"][field]["path"] = "relative.hpp"
    with pytest.raises(FactsToolError, match="E_SCHEMA"):
        MatchResults.from_dict(payload)


@pytest.mark.parametrize("offset", [8, 10, 11])
def test_nonempty_byte_range_is_half_open(offset):
    payload = _payload()
    payload["matches"][0]["bindings"]["symbol"]["location"]["offset"] = offset
    with pytest.raises(FactsToolError, match="E_SCHEMA"):
        MatchResults.from_dict(payload)


def test_unicode_large_offsets_null_locations_and_additive_fields_roundtrip():
    payload = _payload()
    binding = payload["matches"][0]["bindings"]["symbol"]
    for field in ("location", "range"):
        binding[field]["path"] = "/work/示例.hpp"
        binding[field]["offset"] = 2**63
    expression = payload["matches"][0]["bindings"]["expression"]
    expression.update(location=None, range=None,
                      location_unavailable_reason="source unavailable")
    payload["future_additive_field"] = {"enabled": True}
    results = MatchResults.from_json(json.dumps(payload).encode("utf-8"))
    symbol = results.matches[0].bindings["symbol"]
    assert symbol.location.offset == 2**63
    assert symbol.location.path == "/work/示例.hpp"
    assert results.matches[0].bindings["expression"].location is None
    assert MatchResults.from_json(results.to_json()).to_dict() == results.to_dict()
    with pytest.raises(TypeError):
        results.matches[0].bindings["symbol"] = symbol
    with pytest.raises(FrozenInstanceError):
        results.sources = ()


@pytest.mark.parametrize("version", [True, 1.0, "1", 0, 2])
def test_version_requires_supported_integer(version):
    payload = _payload()
    payload["schema_version"] = version
    with pytest.raises(FactsToolError, match="E_SCHEMA"):
        MatchResults.from_dict(payload)
