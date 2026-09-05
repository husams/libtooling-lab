from .rows import Row, public_row, row_key
from .view_loader import ViewLoader


def direct_type_use(types: list[Row], loader: ViewLoader) -> list[Row]:
    targets = {int(row["id"]) for row in types if isinstance(row.get("id"), int)}
    symbols = {int(row["id"]): row for row in loader.load("symbol")}
    result: list[Row] = []
    for edge in loader.load("edge"):
        if (
            edge["kind"]
            in {"of_type", "return_type", "param_type", "template_argument_type"}
            and edge["destination_id"] in targets
        ):
            owner = symbols.get(int(edge["source_id"]))
            if owner:
                result.append(
                    _witness(owner, symbols[int(edge["destination_id"])], edge["kind"])
                )
    for view in ("parameter", "template_parameter", "template_argument"):
        for owner in loader.load(view):
            if owner.get("type_id") in targets:
                target = symbols.get(int(owner["type_id"]))
                if target:
                    result.append(_witness(owner, target, f"{view}_type"))
    return list({row_key(row): row for row in result}.values())


def _witness(owner: Row, target: Row, relation: str) -> Row:
    key = f"type-use:{row_key(owner)}:{row_key(target)}:{relation}"
    return {
        "owner": public_row(owner),
        "type": public_row(target),
        "through": [relation],
        "length": 1,
        "_key": key,
        "_view": "path",
    }
