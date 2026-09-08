import json
import sys
from pathlib import Path

from facts_tool import open_codebase
from facts_tool.queryplan import out, start, symbol

facts, project = map(Path, sys.argv[1:3])
mode = sys.argv[3] if len(sys.argv) > 3 else ""

if mode == "evidence":
    from evidence_fixture import prepare_evidence
    from evidence_queries import run_evidence

    prepare_evidence(facts, project)
    run_evidence(facts, project)
else:
    with open_codebase(facts_db=facts, project_db=project) as cb:
        result = cb.executor.run((start(symbol("app::run")) | out("calls")).plan)
        assert [row["name"] for row in result] == ["save"]
        if mode == "graph":
            assert cb.callgraphs.latest().status == "complete"

print(json.dumps({"package": "facts-tool-query", "query": "pass"}))
