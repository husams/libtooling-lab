"""Strict YAML loading and local JSON pointer access."""
from pathlib import Path
from typing import Any

import yaml


class ContractLoader(yaml.SafeLoader):
    """Reject duplicate keys before validation can silently lose information."""


def mapping(loader: ContractLoader, node: yaml.MappingNode) -> dict[str, Any]:
    result = {}
    for key_node, value_node in node.value:
        key = loader.construct_object(key_node, deep=True)
        if not isinstance(key, str):
            raise TypeError(f"YAML keys must be strings at {key_node.start_mark}")
        if key in result:
            raise ValueError(f"Duplicate YAML key {key!r} at {key_node.start_mark}")
        result[key] = loader.construct_object(value_node, deep=True)
    return result


ContractLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, mapping)


def load(path: Path) -> dict[str, Any]:
    document = yaml.load(path.read_text(encoding="utf-8"), Loader=ContractLoader)
    if not isinstance(document, dict):
        raise TypeError(f"Expected an object in {path}")
    return document


def pointer(document: Any, fragment: str) -> Any:
    if not fragment:
        return document
    if not fragment.startswith("/"):
        raise ValueError(f"Only JSON pointer fragments are supported: #{fragment}")
    value = document
    for part in fragment[1:].split("/"):
        key = part.replace("~1", "/").replace("~0", "~")
        try:
            value = value[int(key)] if isinstance(value, list) else value[key]
        except (KeyError, IndexError, ValueError, TypeError) as error:
            raise ValueError(f"Unresolved reference #{fragment}") from error
    return value
