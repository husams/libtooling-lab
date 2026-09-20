"""Typed extraction, AST matching, and dependency jobs."""

from typing import Literal

import httpx

from .analysis_models import (
    DependencyEdge,
    DependencyResult,
    ExtractionResult,
    FileExtraction,
)
from .job_resource import JobResource
from .jobs import AnalysisJob
from .match_models import MatcherBindings, MatchResult, MatchRow
from .selections import Selection


class Extractions(JobResource[ExtractionResult, FileExtraction]):
    def __init__(self, http: httpx.Client) -> None:
        super().__init__(http, "extract", ExtractionResult, FileExtraction)

    def create(
        self, *, selection: Selection, force: bool = False
    ) -> AnalysisJob[ExtractionResult]:
        return self._create({"selection": selection, "force": force})


class Matches(JobResource[MatchResult, MatchRow]):
    def __init__(self, http: httpx.Client) -> None:
        super().__init__(http, "match", MatchResult, MatchRow)

    def create(
        self,
        *,
        selection: Selection,
        expression: str,
        traversal: Literal["AsIs", "IgnoreUnlessSpelledInSource"] = "AsIs",
        capture_source: bool = False,
        relation_kind: str | None = None,
        bindings: MatcherBindings | None = None,
    ) -> AnalysisJob[MatchResult]:
        return self._create(
            {
                "selection": selection,
                "expression": expression,
                "traversal": traversal,
                "capture_source": capture_source,
                "relation_kind": relation_kind,
                "bindings": bindings,
            }
        )


class Dependencies(JobResource[DependencyResult, DependencyEdge]):
    def __init__(self, http: httpx.Client) -> None:
        super().__init__(http, "dependencies", DependencyResult, DependencyEdge)

    def create(self, *, selection: Selection) -> AnalysisJob[DependencyResult]:
        return self._create({"selection": selection})
