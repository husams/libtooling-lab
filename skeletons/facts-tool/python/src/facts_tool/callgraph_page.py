from collections.abc import Iterator
from dataclasses import dataclass
from typing import TypeVar, overload

T = TypeVar("T")


@dataclass(frozen=True)
class CallGraphPage[T]:
    items: tuple[T, ...]
    total: int
    next_cursor: int | None

    @property
    def complete(self) -> bool:
        return self.next_cursor is None

    @property
    def truncated(self) -> bool:
        return not self.complete

    def __iter__(self) -> Iterator[T]:
        return iter(self.items)

    def __len__(self) -> int:
        return len(self.items)

    @overload
    def __getitem__(self, index: int) -> T: ...

    @overload
    def __getitem__(self, index: slice) -> tuple[T, ...]: ...

    def __getitem__(self, index: int | slice) -> T | tuple[T, ...]:
        return self.items[index]
