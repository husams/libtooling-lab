from .catalog_relations import relation_id
from .rows import Row, public_row, row_key
from .traversal import NeighborFn
from .view_loader import ViewLoader


def find_paths(
    starts: list[Row],
    targets: list[Row],
    relation: str,
    low: int,
    high: int,
    inbound: bool,
    neighbors: NeighborFn,
    budget: int,
) -> tuple[list[Row], bool]:
    target_keys = {row_key(row) for row in targets}
    results: list[Row] = []
    expanded = 0
    for start in starts:
        frontier: list[list[Row]] = [[start]]
        for depth in range(1, high + 1):
            next_paths: list[list[Row]] = []
            for witness in frontier:
                for node in neighbors([witness[-1]], relation, inbound):
                    expanded += 1
                    key = row_key(node)
                    repeated = key in {row_key(item) for item in witness}
                    if repeated and not (key == row_key(start) and key in target_keys):
                        continue
                    candidate = [*witness, node]
                    if depth >= low and key in target_keys:
                        results.append(_path(candidate, relation, inbound))
                    elif not repeated:
                        next_paths.append(candidate)
                    if expanded >= budget:
                        return _minimal(results), True
            if results and any(row["start"]["id"] == start["id"] for row in results):
                break
            frontier = next_paths
    return _minimal(results), False


def _path(nodes: list[Row], relation: str, inbound: bool) -> Row:
    keys = [row_key(row) for row in nodes]
    steps = [
        {
            "relation": relation,
            "inbound": inbound,
            "source_id": nodes[i]["id"],
            "destination_id": nodes[i + 1]["id"],
        }
        for i in range(len(nodes) - 1)
    ]
    return {
        "start": public_row(nodes[0]),
        "end": public_row(nodes[-1]),
        "nodes": [public_row(row) for row in nodes],
        "steps": steps,
        "length": len(nodes) - 1,
        "relation_id": relation_id(relation),
        "_key": "path:" + "/".join(keys),
        "_view": "path",
    }


def _minimal(paths: list[Row]) -> list[Row]:
    best: dict[str, int] = {}
    for row in paths:
        key = str(row["start"]["id"])
        best[key] = min(best.get(key, int(row["length"])), int(row["length"]))
    return sorted(
        (row for row in paths if int(row["length"]) == best[str(row["start"]["id"])]),
        key=lambda row: (int(row["length"]), str(row["_key"])),
    )


def attach_evidence(paths: list[Row], loader: ViewLoader, relation: str) -> None:
    kind = relation_id(relation)
    if kind is None:
        return
    sites = loader.load("site")
    for witness in paths:
        for step in witness["steps"]:
            source, target = step["source_id"], step["destination_id"]
            if step["inbound"]:
                source, target = target, source
            step["sites"] = [
                public_row(site)
                for site in sites
                if site["kind_id"] == kind
                and site["source_id"] == source
                and site["destination_id"] == target
            ]
