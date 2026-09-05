from collections.abc import Callable
from typing import Any


def run_matrix(matrix, operation: Callable[[Any, bool], Any]):
    return [(native, operation(codebase, native)) for native, codebase in matrix]


def matrix_values(results):
    return [value for _, value in results]
