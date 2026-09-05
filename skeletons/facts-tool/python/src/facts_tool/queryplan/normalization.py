from facts_tool.catalog_relations import relation_name

from .types import UnknownPolicy


def unknown_value(value: UnknownPolicy | str) -> str:
    return value.value if isinstance(value, UnknownPolicy) else value


def normalize_relation(value: str) -> str:
    return relation_name(value)
