import os

from .budgets import Budgets
from .codebase import CodeBase
from .database import open_pair
from .executor import Executor
from .pairing import validate_pair
from .provenance import DatabaseIdentity, PairProvenance
from .schema import inspect_schema
from .view_loader import ViewLoader


def open_codebase(
    *,
    facts_db: str | os.PathLike[str],
    project_db: str | os.PathLike[str],
    budgets: Budgets | None = None,
) -> CodeBase:
    facts_path, project_path, facts, project = open_pair(facts_db, project_db)
    try:
        facts_schema = inspect_schema(facts, "facts")
        project_schema = inspect_schema(project, "project")
        validate_pair(facts, project)
        provenance = PairProvenance(
            DatabaseIdentity.from_path(facts_path, facts_schema),
            DatabaseIdentity.from_path(project_path, project_schema),
        )
        loader = ViewLoader(facts, project)
        return CodeBase(
            facts, project, Executor(loader, provenance, budgets), provenance
        )
    except Exception:
        facts.close()
        project.close()
        raise
