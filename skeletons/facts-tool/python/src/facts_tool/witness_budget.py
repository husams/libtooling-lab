from .rows import Row


def cap_witnesses(paths: list[Row], budget: int) -> tuple[list[Row], bool]:
    if budget < 1:
        return [], bool(paths)
    kept: list[Row] = []
    used = 0
    for path in paths:
        cost = len(path.get("nodes", ())) + len(path.get("steps", ()))
        if used + cost > budget:
            return kept, True
        kept.append(path)
        used += cost
    return kept, False
