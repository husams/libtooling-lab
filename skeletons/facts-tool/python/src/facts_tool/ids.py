from dataclasses import dataclass

from .errors import fail

MASK32 = (1 << 32) - 1
MASK64 = (1 << 64) - 1


@dataclass(frozen=True, order=True)
class SymbolId:
    file_id: int
    index: int

    def __post_init__(self) -> None:
        if not 0 <= self.file_id <= MASK32 or not 0 <= self.index <= MASK32:
            fail("E_IDENTITY", "symbol identity halves must be unsigned 32-bit")

    @property
    def packed(self) -> int:
        return (self.file_id << 32) | self.index

    @property
    def sqlite(self) -> int:
        value = self.packed
        return value if value < (1 << 63) else value - (1 << 64)

    def to_dict(self) -> dict[str, str | int]:
        return {
            "packed": str(self.packed),
            "file_id": self.file_id,
            "index": self.index,
        }

    @classmethod
    def unpack(cls, value: int | str) -> "SymbolId":
        packed = int(value) & MASK64
        return cls(packed >> 32, packed & MASK32)


def logical_id(domain: str, value: object) -> str:
    return f"{domain}:{value}"
