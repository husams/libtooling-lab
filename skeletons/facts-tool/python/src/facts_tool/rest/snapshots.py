"""Decode either a structured domain job or a deprecated CLI job snapshot."""

from .decoding import job, object_value
from .domain_decoding import domain_job
from .domain_models import DomainJob
from .models import Job


def snapshot(value: object, *, metadata: bool = False) -> Job | DomainJob:
    body = object_value(value)
    return domain_job(body) if "operation" in body else job(body, metadata=metadata)
