"""Readiness checks distinguish indexing, missing configuration, and failure."""

from .catalog_models import Repository
from .index import IndexStatus
from .watch_models import WatcherStatus


class RepositoryNotReady(RuntimeError):
    def __init__(self, repository_id: str, code: str, message: str) -> None:
        self.repository_id, self.code = repository_id, code
        super().__init__(message)


class RepositoryTimeout(TimeoutError):
    """Waiting stopped; automatic server processing continues."""


def monitoring_ready(
    repo: Repository, watch: WatcherStatus, index: IndexStatus
) -> bool:
    if not watch.enabled:
        raise RepositoryNotReady(
            repo.id,
            "automatic_import_disabled",
            "Automatic repository monitoring is disabled",
        )
    if repo.active_clone_id is None:
        raise RepositoryNotReady(
            repo.id, "needs_configuration", "Repository has no active clone"
        )
    visible = any(c.clone_id == repo.active_clone_id and c.active for c in watch.clones)
    if watch.active or watch.pending or watch.scanning or not visible:
        return False
    if watch.last_error:
        raise RepositoryNotReady(repo.id, "processing_failed", watch.last_error)
    if index.state == "failed":
        raise RepositoryNotReady(repo.id, "index_failed", index.error or "Index failed")
    return (
        watch.ready
        and (watch.cycles > 0 or watch.resumed)
        and index.state == "ready"
        and not index.pending
        and index.index_revision is not None
    )


def sources_ready(repo: Repository) -> bool:
    if repo.source_count == 0:
        raise RepositoryNotReady(
            repo.id,
            "needs_configuration",
            "Repository has no registered compilation commands",
        )
    return repo.indexed_source_count == repo.source_count
