import math

from .catalog_kinds import SYMBOL_KINDS
from .ids import MASK64, SymbolId

SqlValue = str | int | float | bytes | None
SqlPredicate = tuple[str, tuple[SqlValue, ...]]


def _integer(value: object, unsigned: bool) -> int | None:
    if not isinstance(value, (int, float)):
        return None
    if isinstance(value, float) and (
        not math.isfinite(value) or not value.is_integer()
    ):
        return None
    number = int(value)
    low, high = (0, MASK64) if unsigned else (-(1 << 63), (1 << 63) - 1)
    return number if low <= number <= high else None


def symbol_equality(field: str, value: object) -> SqlPredicate | None:
    if type(value) not in {str, int, float, bool, bytes, type(None)}:
        return None
    if field in {"id", "kind_id"}:
        number = _integer(value, field == "id")
        if number is None:
            return "0", ()
        column = "id" if field == "id" else "kind"
        bound = SymbolId.unpack(number).sqlite if field == "id" else number
        return f"s.{column} IS ?", (bound,)
    if field == "kind":
        if not isinstance(value, str):
            return "0", ()
        if value in SYMBOL_KINDS:
            return "s.kind IS ?", (SYMBOL_KINDS.index(value),)
        try:
            number = int(value.removeprefix("kind_"))
        except ValueError:
            return "0", ()
        if value != f"kind_{number}" or 0 <= number < len(SYMBOL_KINDS):
            return "0", ()
        return symbol_equality("kind_id", number)
    if field in {"usr", "qualified_name", "qual_name"}:
        column = "usr" if field == "usr" else "qualified_name"
        if value is None:
            return f"s.{column} IS NULL", ()
        if not isinstance(value, str):
            return "0", ()
        return f"s.{column} IS ?", (value,)
    return None
