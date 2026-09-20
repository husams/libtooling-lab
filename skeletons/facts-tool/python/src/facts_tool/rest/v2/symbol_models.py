"""Global symbols and their typed occurrences and relations."""

from dataclasses import dataclass
from enum import StrEnum
from typing import Literal


class SymbolKind(StrEnum):
    UNKNOWN = "unknown"
    MODULE = "module"
    NAMESPACE = "namespace"
    NAMESPACE_ALIAS = "namespace_alias"
    MACRO = "macro"
    INCLUDE_DIRECTIVE = "include_directive"
    ENUM = "enum"
    STRUCT = "struct"
    CLASS = "class"
    PROTOCOL = "protocol"
    EXTENSION = "extension"
    UNION = "union"
    TYPE_ALIAS = "type_alias"
    FUNCTION = "function"
    VARIABLE = "variable"
    FIELD = "field"
    ENUM_CONSTANT = "enum_constant"
    INSTANCE_METHOD = "instance_method"
    CLASS_METHOD = "class_method"
    STATIC_METHOD = "static_method"
    INSTANCE_PROPERTY = "instance_property"
    CLASS_PROPERTY = "class_property"
    STATIC_PROPERTY = "static_property"
    CONSTRUCTOR = "constructor"
    DESTRUCTOR = "destructor"
    CONVERSION_FUNCTION = "conversion_function"
    PARAMETER = "parameter"
    USING = "using"
    TEMPLATE_TYPE_PARM = "template_type_parm"
    TEMPLATE_TEMPLATE_PARM = "template_template_parm"
    NON_TYPE_TEMPLATE_PARM = "non_type_template_parm"
    CONCEPT = "concept"


@dataclass(frozen=True, slots=True)
class SourceLocation:
    file_id: str
    path: str
    line: int | None
    column: int | None


@dataclass(frozen=True, slots=True)
class Symbol:
    symbol_id: str
    qualified_name: str
    kind: str
    usr: str
    repository: str
    component: str
    definition: SourceLocation | None


@dataclass(frozen=True, slots=True)
class SymbolOccurrence:
    kind: Literal["declaration", "definition"]
    file_id: str
    path: str
    line: int | None
    column: int | None
    offset: int | None
    size: int | None


@dataclass(frozen=True, slots=True)
class RelatedSymbol:
    symbol_id: str
    qualified_name: str
    usr: str


@dataclass(frozen=True, slots=True)
class SymbolRelation:
    kind: str
    source: RelatedSymbol
    target: RelatedSymbol
    count: int
    position: int
