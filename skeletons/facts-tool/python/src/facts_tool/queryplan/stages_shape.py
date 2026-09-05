from collections.abc import Sequence

from .types import Stage


def select(fields: Sequence[str]) -> Stage:
    return Stage("select", fields=tuple(fields))


def count() -> Stage:
    return Stage("count")


def distinct() -> Stage:
    return Stage("distinct")


def order_by(fields: Sequence[str]) -> Stage:
    return Stage("order_by", fields=tuple(fields))


def limit(n: int) -> Stage:
    return Stage("limit", n=n)
