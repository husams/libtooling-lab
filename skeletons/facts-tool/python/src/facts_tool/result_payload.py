import json
from typing import TYPE_CHECKING, Any, cast

from .rows import public_row

if TYPE_CHECKING:
    from .result import Result


def _safe(value: Any) -> Any:
    if isinstance(value, int) and abs(value) > (1 << 53) - 1:
        return str(value)
    if isinstance(value, dict):
        return {key: _safe(item) for key, item in value.items()}
    if isinstance(value, list):
        return [_safe(item) for item in value]
    return value


def payload(result: "Result") -> dict[str, Any]:
    result.materialize()
    data: dict[str, Any] = {
        "shape": result.shape,
        "view": result.view,
        "truncated": result.truncated,
        "partial": result.partial,
        "unknown": result.unknown,
        "cursor": result.cursor,
        "provenance": result.provenance.to_dict(),
    }
    if result.shape == "scalar":
        data["scalar"] = result.scalar
    else:
        key = {"nodes": "nodes", "rows": "rows", "path": "paths"}[result.shape]
        data[key] = [public_row(row) for row in result.values]
    return cast(dict[str, Any], _safe(data))


def serialized(result: "Result") -> str:
    return json.dumps(payload(result), ensure_ascii=False, sort_keys=True)
