from .types import Plan, Query, Source


def start(source: Source | None = None) -> Query:
    return Query(Plan(source or codebase()))


def codebase() -> Source:
    return Source("codebase")


def symbol(ref: str) -> Source:
    return Source("symbol", ref)


def entity(ref: str) -> Source:
    return Source("entity", ref)
