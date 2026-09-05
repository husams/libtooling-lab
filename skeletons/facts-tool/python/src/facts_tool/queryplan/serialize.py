import json
from dataclasses import fields, is_dataclass
from enum import Enum
from typing import Any

from .types import Plan


def _plain(value: Any) -> Any:
    if is_dataclass(value):
        return {
            field.name: _plain(getattr(value, field.name)) for field in fields(value)
        }
    if isinstance(value, Enum):
        return value.value
    if isinstance(value, tuple):
        return [_plain(item) for item in value]
    if isinstance(value, dict):
        return {key: _plain(item) for key, item in sorted(value.items())}
    return value


def plan_to_dict(plan: Plan) -> dict[str, Any]:
    value = _plain(plan)
    if not isinstance(value, dict):
        raise TypeError("plan serialization did not produce an object")
    return value


def canonical_json(plan: Plan) -> str:
    return json.dumps(
        plan_to_dict(plan), sort_keys=True, separators=(",", ":"), ensure_ascii=False
    )
