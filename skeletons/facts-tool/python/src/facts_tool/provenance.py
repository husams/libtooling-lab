import os
from dataclasses import dataclass
from pathlib import Path

from .schema import SchemaIdentity


@dataclass(frozen=True)
class DatabaseIdentity:
    path: str
    device: int
    inode: int
    size: int
    mtime_ns: int
    schema: SchemaIdentity

    @classmethod
    def from_path(cls, path: Path, schema: SchemaIdentity) -> "DatabaseIdentity":
        stat = os.stat(path)
        return cls(
            str(path), stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns, schema
        )

    def to_dict(self) -> dict[str, object]:
        return {
            "path": self.path,
            "device": self.device,
            "inode": self.inode,
            "size": self.size,
            "mtime_ns": self.mtime_ns,
            "schema": self.schema.to_dict(),
        }


@dataclass(frozen=True)
class PairProvenance:
    facts: DatabaseIdentity
    project: DatabaseIdentity
    pairing: str = "unverifiable"

    def to_dict(self) -> dict[str, object]:
        return {
            "facts": self.facts.to_dict(),
            "project": self.project.to_dict(),
            "pairing": self.pairing,
        }
