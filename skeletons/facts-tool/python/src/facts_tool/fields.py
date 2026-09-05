from .catalog_views import SYMBOL_FIELDS
from .fields_facts import FACT_FIELDS
from .fields_project import PROJECT_FIELDS

FIELDS = {"symbol": SYMBOL_FIELDS | {"identity", "node_kind"}}
FIELDS.update(FACT_FIELDS)
FIELDS.update(PROJECT_FIELDS)
