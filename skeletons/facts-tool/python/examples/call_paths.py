from common import database_paths

from facts_tool import open_codebase
from facts_tool.queryplan import out, path, rank, start, symbol

facts, project = database_paths("Traverse calls and show a witness")
with open_codebase(facts_db=facts, project_db=project) as cb:
    reachable = start(symbol("app::run")) | out("calls", 1, 3)
    witness = (
        start(symbol("app::run"))
        | path(start(symbol("app::persist")), "calls")
        | rank(5)
    )
    print(cb.executor.run(reachable.plan).to_json())
    print(cb.executor.run(witness.plan).to_json())
