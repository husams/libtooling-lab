from typing import Any

from .errors import fail
from .match_models import MatchBinding, MatchLocation, MatchRange, MatchResult
from .match_validation import (
    as_mapping,
    integer,
    nullable_string,
    required,
    string,
)


def _location(value: Any, field: str) -> MatchLocation | None:
    if value is None:
        return None
    data = as_mapping(value, field)
    return MatchLocation(
        string(required(data, "path"), f"{field}.path", absolute=True),
        integer(required(data, "line"), f"{field}.line", 1),
        integer(required(data, "column"), f"{field}.column", 1),
        integer(required(data, "offset"), f"{field}.offset"),
    )


def _range(value: Any, field: str) -> MatchRange | None:
    if value is None:
        return None
    data = as_mapping(value, field)
    return MatchRange(
        string(required(data, "path"), f"{field}.path", absolute=True),
        integer(required(data, "offset"), f"{field}.offset"),
        integer(required(data, "size"), f"{field}.size"),
    )


def binding(value: Any, field: str) -> MatchBinding:
    data = as_mapping(value, field)
    location = _location(required(data, "location"), f"{field}.location")
    result_range = _range(required(data, "range"), f"{field}.range")
    reason = nullable_string(
        required(data, "location_unavailable_reason"),
        f"{field}.location_unavailable_reason",
    )
    if (location is None) != (reason is not None):
        fail("E_SCHEMA", f"{field} location and unavailable reason disagree")
    if location and result_range:
        if location.path != result_range.path:
            fail("E_SCHEMA", f"{field} location and range paths differ")
        end = result_range.offset + result_range.size
        within = (location.offset == result_range.offset if result_range.size == 0
                  else result_range.offset <= location.offset < end)
        if not within:
            fail("E_SCHEMA", f"{field} location offset is outside range")
    return MatchBinding(
        string(required(data, "node_kind"), f"{field}.node_kind"),
        nullable_string(required(data, "name"), f"{field}.name"),
        nullable_string(required(data, "usr"), f"{field}.usr"),
        location,
        result_range,
        reason,
    )


def match(value: Any, index: int) -> MatchResult:
    data = as_mapping(value, f"matches[{index}]")
    bindings_data = as_mapping(
        required(data, "bindings"), f"matches[{index}].bindings"
    )
    bindings = {
        string(key, f"matches[{index}].bindings key"): binding(
            item, f"matches[{index}].bindings.{key}"
        )
        for key, item in bindings_data.items()
    }
    return MatchResult(
        string(
            required(data, "translation_unit"), "translation_unit", absolute=True
        ),
        nullable_string(required(data, "relation_kind"), "relation_kind"),
        bindings,
    )
