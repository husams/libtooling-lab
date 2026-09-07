"""Expected S-024 path evidence and node-simple path reconstruction (B-042)."""

CALLS, DISPATCH = 1, 18
SOURCE, TARGET = "s024_fixture::source", "s024_fixture::target"

# Every relation_site the all-simple search retains: the four node-simple
# source -> target paths of the fixture and nothing else (the alpha -> source
# back edge is a cycle and must never be persisted).
ALL_SIMPLE_EDGES = {
    (SOURCE, "s024_fixture::alpha", CALLS),
    ("s024_fixture::alpha", TARGET, CALLS),
    (SOURCE, "s024_fixture::beta", CALLS),
    ("s024_fixture::beta", TARGET, CALLS),
    (SOURCE, "s024_fixture::dispatch", CALLS),
    ("s024_fixture::dispatch", "s024_fixture::Left::invoke", DISPATCH),
    ("s024_fixture::dispatch", "s024_fixture::Right::invoke", DISPATCH),
    ("s024_fixture::Left::invoke", TARGET, CALLS),
    ("s024_fixture::Right::invoke", TARGET, CALLS),
}
ALL_SIMPLE_PATHS = {
    (SOURCE, "s024_fixture::alpha", TARGET),
    (SOURCE, "s024_fixture::beta", TARGET),
    (SOURCE, "s024_fixture::dispatch", "s024_fixture::Left::invoke", TARGET),
    (SOURCE, "s024_fixture::dispatch", "s024_fixture::Right::invoke", TARGET),
}
SHORTEST_EDGES = {(SOURCE, "s024_fixture::alpha", CALLS),
                  ("s024_fixture::alpha", TARGET, CALLS)}


def edge_triples(edges):
    return {(edge["source"], edge["target"], edge["kind"]) for edge in edges}


def simple_paths(edges, source, target):
    """Node-simple source -> target paths over the persisted edge subgraph."""
    outgoing = {}
    for edge in edges:
        outgoing.setdefault(edge["source"], set()).add(edge["target"])
    found = set()

    def walk(node, path):
        if node == target:
            found.add(tuple(path))
            return
        for nxt in sorted(outgoing.get(node, ())):
            if nxt not in path:
                walk(nxt, path + [nxt])

    walk(source, [source])
    return found


def edges_on_paths(paths):
    return {(path[i], path[i + 1]) for path in paths for i in range(len(path) - 1)}
