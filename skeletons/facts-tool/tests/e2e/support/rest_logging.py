"""Assertions for complete public JSON log records, never private logger state."""
import json


def records(path):
    if not path.exists():
        return []
    text = path.read_text()
    lines = text.splitlines()
    if text and not text.endswith("\n"):
        lines = lines[:-1]
    result = [json.loads(line) for line in lines if line.strip()]
    for record in result:
        assert isinstance(record["timestamp"], int)
        assert isinstance(record["fields"], dict)
    return result


def events(path):
    return {record["event"] for record in records(path)}
