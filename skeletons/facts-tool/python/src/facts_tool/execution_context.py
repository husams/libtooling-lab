from dataclasses import dataclass

from .budgets import Budgets
from .neighbors import Neighbors
from .view_loader import ViewLoader


@dataclass
class ExecutionContext:
    loader: ViewLoader
    neighbors: Neighbors
    budgets: Budgets
    after_id: str | int | None = None
    enumerated: bool = False
