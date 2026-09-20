"""Read the public JSON-lines logging format, including during async writes."""
import json


def records(path):
    if not path.exists():
        return []
    content = path.read_text()
    lines = content.splitlines()
    if content and not content.endswith("\n"):
        lines = lines[:-1]
    result = [json.loads(line) for line in lines if line.strip()]
    for record in result:
        assert isinstance(record["timestamp"], int)
        assert isinstance(record["event"], str)
        assert isinstance(record["fields"], dict)
        assert record["level"] in {"error", "warning", "info", "debug", "trace"}
    return result


def events(path):
    return {record["event"] for record in records(path)}
