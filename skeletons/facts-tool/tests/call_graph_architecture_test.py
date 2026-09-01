from __future__ import annotations

import sys
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    root = Path(sys.argv[1])
    owned = [
        *sorted(path for path in (root / "src/analysis/callgraph").glob("*")
                if path.suffix in {".cpp", ".hpp", ".h"}),
        *sorted((root / "src/ast/extractors").glob("CallSite.*")),
        *sorted((root / "src/ast/extractors").glob("ReceiverContext.*")),
        *sorted((root / "src/ast/extractors").glob("OverrideRelation.*")),
        *sorted((root / "src/ast/visitors").glob("CallGraphVisitor.*")),
        *sorted((root / "src/commands/analyse").glob("CallGraphCommand.*")),
    ]
    for path in owned:
        require(len(path.read_text(encoding="utf-8").splitlines()) <= 100,
                f"{path.relative_to(root)} exceeds 100 physical lines")
    visitor = root / "src/ast/visitors/CallGraphVisitor.cpp"
    visitor_text = visitor.read_text(encoding="utf-8")
    require("clang::CallGraph graph" in visitor_text and
            "graph.getRoot()" in visitor_text and "node->getDecl()" in visitor_text and
            "node->callees()" in visitor_text,
            "CallGraphVisitor does not own the Clang graph")
    require("extractCallSite" in visitor_text and
            "extractOverrideRelations" in visitor_text,
            "CallGraphVisitor does not delegate extraction")
    forbidden = ("RecursiveASTVisitor", "TraverseDecl", "TraverseStmt",
                 "dataTraverseStmtPre", "WalkUpFrom")
    for path in owned:
        text = path.read_text(encoding="utf-8")
        require(not any(token in text for token in forbidden),
                f"{path.relative_to(root)} performs a second traversal")
    owners = []
    for path in (root / "src").rglob("*"):
        if path.suffix not in {".cpp", ".hpp", ".h"}:
            continue
        if "addToCallGraph" in path.read_text(encoding="utf-8"):
            owners.append(path)
    require(owners == [visitor], "CallGraph traversal has multiple owners")
    typed = (root / "src/analysis/callgraph/RelationSiteContextStore.cpp")
    require("ReceiverCertainty::Exact" in typed.read_text(encoding="utf-8"),
            "relation-site context access is not typed")


if __name__ == "__main__":
    main()
