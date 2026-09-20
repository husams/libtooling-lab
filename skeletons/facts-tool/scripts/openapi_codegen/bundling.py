"""Bundle local references without fetching network resources."""
from pathlib import Path
from urllib.parse import unquote, urlsplit

from .loading import load, pointer


def location(reference: str, current: Path, root: Path) -> tuple[Path, str]:
    url = urlsplit(reference)
    if url.scheme or url.netloc or url.query:
        raise ValueError(f"Only local contract references are allowed: {reference}")
    path = (current.parent / unquote(url.path)).resolve() if url.path else current
    if not path.is_relative_to(root):
        raise ValueError(f"Reference escapes the contract directory: {reference}")
    return path, unquote(url.fragment)


def bundle(source: Path) -> dict:
    source = source.resolve()
    root, documents = source.parent, {source: load(source)}
    aliases = {}
    components = documents[source].get("components", {})
    if not isinstance(components, dict):
        raise TypeError("components must be an object")
    for category, entries in components.items():
        if not isinstance(entries, dict):
            raise TypeError(f"components.{category} must be an object")
        for name, entry in entries.items():
            reference = entry.get("$ref", "") if isinstance(entry, dict) else ""
            if not isinstance(reference, str):
                raise TypeError("$ref must be a string")
            if reference and not reference.startswith("#"):
                aliases[location(reference, source, root)] = f"#/components/{category}/{name}"

    def walk(value, current: Path, trail=(), position=""):
        if isinstance(value, list):
            return [walk(item, current, trail) for item in value]
        if not isinstance(value, dict):
            return value
        siblings = {key: walk(item, current, trail, f"{position}/{key}")
                    for key, item in value.items() if key != "$ref"}
        reference = value.get("$ref")
        if reference is None:
            return siblings
        if not isinstance(reference, str):
            raise TypeError("$ref must be a string")
        target = location(reference, current, root)
        if target[0] == source:
            return {"$ref": "#" + target[1], **siblings}
        alias = aliases.get(target)
        if alias and (alias != f"#{position}" or target in trail):
            return {"$ref": alias, **siblings}
        if target in trail:
            raise ValueError(f"Circular external reference needs a root component: {reference}")
        path, fragment = target
        if path not in documents:
            documents[path] = load(path)
        resolved = walk(pointer(documents[path], fragment), path, (*trail, target))
        if not isinstance(resolved, dict):
            raise TypeError(f"Reference must resolve to an object: {reference}")
        return {**resolved, **siblings}

    result = walk(documents[source], source)

    def verify(value):
        if isinstance(value, list):
            for item in value:
                verify(item)
        if isinstance(value, dict):
            if "$ref" in value:
                pointer(result, unquote(value["$ref"][1:]))
            for item in value.values():
                verify(item)

    verify(result)
    return result
