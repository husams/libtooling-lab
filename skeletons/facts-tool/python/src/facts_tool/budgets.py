from dataclasses import dataclass


@dataclass(frozen=True)
class Budgets:
    enumeration: int = 10_000
    traversal: int = 10_000
    result_cap: int = 1_000
    max_depth: int = 32
    path_expansion: int = 10_000
    witness_reconstruction: int = 200_000

    def to_dict(self) -> dict[str, int]:
        return {key: int(value) for key, value in vars(self).items()}
