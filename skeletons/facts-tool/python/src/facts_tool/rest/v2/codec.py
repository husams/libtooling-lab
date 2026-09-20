"""Strict dataclass decoding and omission-aware JSON serialization."""

from dataclasses import MISSING, fields, is_dataclass
from enum import Enum
from functools import cache
from types import UnionType
from typing import (
    Any,
    Literal,
    TypeVar,
    Union,
    cast,
    get_args,
    get_origin,
    get_type_hints,
)

from ..errors import ProtocolError

T = TypeVar("T")


class Unset(Enum):
    VALUE = "unset"


UNSET = Unset.VALUE


@cache
def model_hints(model: type[object]) -> dict[str, Any]:
    return get_type_hints(model)


def encode(value: object) -> Any:
    if is_dataclass(value) and not isinstance(value, type):
        return {
            f.name: encode(v)
            for f in fields(value)
            if (v := getattr(value, f.name)) is not None and v is not UNSET
        }
    if isinstance(value, dict):
        return {k: encode(v) for k, v in value.items() if v is not UNSET}
    if isinstance(value, tuple | list):
        return [encode(v) for v in value]
    if isinstance(value, Enum):
        return value.value
    return value


def decode[T](model: type[T], value: object) -> T:
    return cast(T, _decode(model, value))


def _decode(model: Any, value: object) -> Any:
    origin, args = get_origin(model), get_args(model)
    if origin in (UnionType, Union):
        for option in args:
            try:
                return _decode(option, value)
            except ProtocolError:
                pass
        raise ProtocolError(f"Value does not match {model}")
    if origin is Literal:
        if value in args and type(value) in {type(x) for x in args}:
            return value
        raise ProtocolError(f"Invalid enum value for {model}: {value!r}")
    if origin in (tuple, list):
        if not isinstance(value, list):
            raise ProtocolError("Expected JSON array")
        items = [_decode(args[0], item) for item in value]
        return tuple(items) if origin is tuple else items
    if origin is dict:
        if not isinstance(value, dict):
            raise ProtocolError("Expected JSON object")
        return {_decode(args[0], k): _decode(args[1], v) for k, v in value.items()}
    if isinstance(model, type) and is_dataclass(model):
        if not isinstance(value, dict):
            raise ProtocolError(f"Expected object for {model.__name__}")
        hints = model_hints(model)
        result = {}
        for f in fields(model):
            if f.name in value:
                result[f.name] = _decode(hints[f.name], value[f.name])
            elif f.default is MISSING and f.default_factory is MISSING:
                raise ProtocolError(f"Missing {model.__name__}.{f.name}")
        return model(**result)
    if model is Any or model is object:
        return value
    if model is type(None) and value is None:
        return None
    if isinstance(model, type) and issubclass(model, Enum):
        try:
            return model(value)
        except ValueError as error:
            raise ProtocolError(f"Invalid {model.__name__}") from error
    if type(value) is model:
        return value
    raise ProtocolError(f"Expected {model}, received {type(value).__name__}")
