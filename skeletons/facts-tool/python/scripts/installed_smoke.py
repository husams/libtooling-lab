import json
import sys
from pathlib import Path

from facts_tool import open_codebase
from facts_tool.queryplan import out, start, symbol

facts, project = map(Path, sys.argv[1:3])
with open_codebase(facts_db=facts, project_db=project) as cb:
    result = cb.executor.run((start(symbol("app::run")) | out("calls")).plan)
    assert [row["name"] for row in result] == ["save"]
    print(json.dumps({"package": "facts-tool-query", "query": "pass"}))
