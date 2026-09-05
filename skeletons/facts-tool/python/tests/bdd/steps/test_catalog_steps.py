from pytest_bdd import scenarios

from .catalog_core_steps import *  # noqa: F403
from .catalog_evidence_steps import *  # noqa: F403
from .catalog_project_steps import *  # noqa: F403

scenarios("../features/catalog.feature")
