import ast
import importlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "src" / "facts_tool"


def module_name(path: Path) -> str:
    relative = path.relative_to(ROOT)
    parts = list(relative.with_suffix("").parts)
    if parts[-1] == "__init__":
        parts.pop()
    return ".".join(("facts_tool", *parts))


def relative_target(owner: str, path: Path, node: ast.ImportFrom) -> str:
    package = owner if path.name == "__init__.py" else owner.rpartition(".")[0]
    parts = package.split(".")
    prefix = parts[: len(parts) - node.level + 1]
    return ".".join((*prefix, *(node.module or "").split("."))).rstrip(".")


def dependencies(paths: list[Path]) -> dict[str, set[str]]:
    modules = {module_name(path): path for path in paths}
    graph = {name: set() for name in modules}
    for owner, path in modules.items():
        for node in ast.parse(path.read_text(encoding="utf-8")).body:
            if not isinstance(node, ast.ImportFrom) or not node.level:
                continue
            target = relative_target(owner, path, node)
            if target in modules:
                graph[owner].add(target)
            for alias in node.names:
                child = f"{target}.{alias.name}"
                if child in modules:
                    graph[owner].add(child)
    return graph


def find_cycle(graph: dict[str, set[str]]) -> list[str]:
    active: list[str] = []
    complete: set[str] = set()

    def visit(node: str) -> list[str]:
        if node in active:
            return active[active.index(node) :] + [node]
        if node in complete:
            return []
        active.append(node)
        for child in graph[node]:
            cycle = visit(child)
            if cycle:
                return cycle
        active.pop()
        complete.add(node)
        return []

    for node in graph:
        cycle = visit(node)
        if cycle:
            return cycle
    return []


def main() -> int:
    paths = sorted(ROOT.rglob("*.py"))
    cycle = find_cycle(dependencies(paths))
    if cycle:
        print("runtime import cycle: " + " -> ".join(cycle))
        return 1
    for path in paths:
        importlib.import_module(module_name(path))
    print(f"{len(paths)} modules import without runtime cycles")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
